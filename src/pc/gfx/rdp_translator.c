/**
 * rdp_translator.c - Translates N64 RDP display lists to OpenGL calls
 *
 * This walks F3DEX2/RDP display lists built by the game code and issues
 * equivalent OpenGL commands. Only a subset of commands are implemented;
 * unhandled commands are silently skipped.
 */

#include <GL/glew.h>
#include <stdio.h>
#include <string.h>
#include <math.h>

// We need the game's GBI definitions for Gfx type and command opcodes
#include "common.h"
#include "PR/gbi.h"

// N64 Mtx conversion (from gu_math_pc.c)
extern void guMtxL2F(float mf[4][4], Mtx *m);

// ============================================================================
// RSP State (geometry pipeline)
// ============================================================================

#define RSP_MAX_VERTICES 32
#define RSP_MTX_STACK_SIZE 16

// Transformed vertex ready for rendering
typedef struct {
    f32 pos[4];    // clip-space position (x, y, z, w)
    f32 color[4];  // RGBA color (0-1)
    f32 tc[2];     // texture coordinates (s, t)
    f32 fog;       // fog factor (0=no fog, 1=full fog)
} RSPVertex;

typedef struct {
    // Vertex buffer (F3DEX2 supports 32 vertices)
    RSPVertex vtxBuf[RSP_MAX_VERTICES];

    // Matrix stacks
    f32 modelview[RSP_MTX_STACK_SIZE][4][4];
    s32 modelviewTop;
    f32 projection[RSP_MTX_STACK_SIZE][4][4];
    s32 projectionTop;

    // Combined MVP matrix (projection * modelview)
    f32 mvp[4][4];
    s32 mvpDirty;

    // Viewport
    f32 vpScaleX, vpScaleY, vpScaleZ;
    f32 vpTransX, vpTransY, vpTransZ;

    // Geometry mode
    u32 geometryMode;

    // Fog
    f32 fogR, fogG, fogB, fogA;
    s16 fogMul, fogOfs;

    // Texture (set by gSPTexture / G_TEXTURE)
    s32 texEnabled;
    u32 texScaleS, texScaleT; // 0.16 fixed point (0xFFFF ≈ 1.0)
    u32 texTile;              // Render tile index (0-7)
    u32 texLevel;             // LOD level
} RSPState;

static RSPState sRSP;

// ============================================================================
// RDP State (rasterization)
// ============================================================================

// N64 tile descriptor (8 tile slots)
typedef struct {
    u32 fmt;         // G_IM_FMT_* (RGBA, CI, IA, I)
    u32 siz;         // G_IM_SIZ_* (4b, 8b, 16b, 32b)
    u32 line;        // TMEM line size (64-bit words per row)
    u32 tmem;        // TMEM offset (in 64-bit words, 0-511)
    u32 palette;     // Palette index (0-15, for CI formats)
    u32 cms, cmt;    // Clamp/mirror/wrap S,T
    u32 masks, maskt;
    u32 shifts, shiftt;
    f32 uls, ult, lrs, lrt; // Tile bounds (quarter-texel units from G_SETTILESIZE)
    s32 sizeSet;     // TRUE if G_SETTILESIZE has been called for this tile
} TileDescriptor;

#define TMEM_SIZE 4096  // 4KB texture memory
#define N64_TILE_COUNT 8

typedef struct {
    // Fill color (RGBA 5551 packed into 32 bits, duplicated for 16-bit mode)
    u32 fillColor;
    // Extracted fill color as floats
    f32 fillR, fillG, fillB, fillA;

    // Primitive color (set by gDPSetPrimColor)
    f32 primR, primG, primB, primA;

    // Environment color (set by gDPSetEnvColor)
    f32 envR, envG, envB, envA;

    // Cycle type (raw 2-bit value: 0=1CYCLE, 1=2CYCLE, 2=COPY, 3=FILL)
    u32 cycleType;

    // Combine mode
    u32 combineHi, combineLo;

    // Other mode bits
    u32 otherModeH;
    u32 otherModeL;

    // Render target tracking
    uintptr_t cimgAddr;   // Current color image address (G_SETCIMG)
    uintptr_t zimgAddr;   // Current depth image address (G_SETZIMG)
    s32 renderingToZbuf;  // TRUE if cimgAddr == zimgAddr (zbuffer fill)

    // Texture image (set by G_SETTIMG)
    uintptr_t texImgAddr;  // Source texture data in DRAM
    u32 texImgFmt;         // G_IM_FMT_*
    u32 texImgSiz;         // G_IM_SIZ_*
    u32 texImgWidth;       // Width in texels

    // Tile descriptors (8 tiles)
    TileDescriptor tiles[N64_TILE_COUNT];

    // TMEM (4KB texture memory - simulated)
    u8 tmem[TMEM_SIZE];

    // Texture lookup table mode (G_TT_NONE, G_TT_RGBA16, G_TT_IA16)
    u32 tlutMode;

    // Blend color (set by gDPSetBlendColor)
    f32 blendR, blendG, blendB, blendA;
} RDPState;

static RDPState sRDP;
static u32 sFrameNum = 0;

// OpenGL textures for each tile slot (recreated as needed)
static GLuint sTileTextures[N64_TILE_COUNT];
static s32 sTileTexWidth[N64_TILE_COUNT];
static s32 sTileTexHeight[N64_TILE_COUNT];
static u32 sTileTexGeneration[N64_TILE_COUNT]; // tracks when texture was uploaded
static u32 sTmemGeneration = 0; // incremented on every TMEM write

// Temporary buffer for converted RGBA8 texture data (max TMEM = 4096 bytes =
// 2048 RGBA16 texels → 2048 * 4 = 8192 bytes RGBA8, or 4096 I8 → 16384 RGBA8)
#define TEX_CONV_BUF_SIZE (TMEM_SIZE * 4)
static u8 sTexConvBuf[TEX_CONV_BUF_SIZE];

// ============================================================================
// Shaders
// ============================================================================

// -- Rectangle shader (pre-transformed 2D) --
static GLuint sRectProgram = 0;
static GLuint sRectVAO = 0;
static GLuint sRectVBO = 0;
static GLint  sRectColorLoc = -1;

// -- Triangle shader (3D geometry with MVP + textures + combiner) --
static GLuint sTriProgram = 0;
static GLuint sTriVAO = 0;
static GLuint sTriVBO = 0;
static GLint  sTriUseTexLoc = -1;     // uniform: uUseTexture
static GLint  sTriTexSamplerLoc = -1; // uniform: uTexture
static GLint  sTriPrimColorLoc = -1;  // uniform: uPrimColor
static GLint  sTriEnvColorLoc = -1;   // uniform: uEnvColor
static GLint  sTriCombColorLoc = -1;  // uniform: uCombineColor (ivec4: a,b,c,d source IDs)
static GLint  sTriCombAlphaLoc = -1;  // uniform: uCombineAlpha (ivec4: a,b,c,d source IDs)
static GLint  sTriFogColorLoc = -1;   // uniform: uFogColor
static GLint  sTriUseFogLoc = -1;     // uniform: uUseFog
// Max triangles we can batch before flushing
#define TRI_BATCH_MAX 256
// 3 vertices per triangle, each has: pos(4) + color(4) + tc(2) + fog(1) = 11 floats
#define TRI_VERTEX_FLOATS 11
static f32 sTriBatch[TRI_BATCH_MAX * 3 * TRI_VERTEX_FLOATS];
static s32 sTriBatchCount = 0;
// Current texture state for the batch (all tris in a batch share the same texture)
static s32 sTriBatchTextured = FALSE;
static GLuint sTriBatchTexId = 0;
// Current combiner state for the batch
static s32 sTriBatchCombColor[4] = {0, 0, 0, 0}; // a,b,c,d source IDs
static s32 sTriBatchCombAlpha[4] = {0, 0, 0, 0};
static f32 sTriBatchPrimColor[4] = {0, 0, 0, 1};
static f32 sTriBatchEnvColor[4] = {0, 0, 0, 1};
// Current render mode for the batch
static u32 sTriBatchOtherModeL = 0;

static s32 sShadersReady = FALSE;

static const char* sRectVertSrc =
    "#version 330 core\n"
    "layout(location = 0) in vec2 aPos;\n"
    "void main() {\n"
    "    gl_Position = vec4(aPos, 0.0, 1.0);\n"
    "}\n";

static const char* sRectFragSrc =
    "#version 330 core\n"
    "uniform vec4 uColor;\n"
    "out vec4 FragColor;\n"
    "void main() {\n"
    "    FragColor = uColor;\n"
    "}\n";

// Triangle vertex shader: clip-space passthrough with texture coords + fog
static const char* sTriVertSrc =
    "#version 330 core\n"
    "layout(location = 0) in vec4 aPos;\n"
    "layout(location = 1) in vec4 aColor;\n"
    "layout(location = 2) in vec2 aTexCoord;\n"
    "layout(location = 3) in float aFog;\n"
    "out vec4 vColor;\n"
    "out vec2 vTexCoord;\n"
    "out float vFog;\n"
    "void main() {\n"
    "    gl_Position = aPos;\n"
    "    vColor = aColor;\n"
    "    vTexCoord = aTexCoord;\n"
    "    vFog = aFog;\n"
    "}\n";

static const char* sTriFragSrc =
    "#version 330 core\n"
    "in vec4 vColor;\n"
    "in vec2 vTexCoord;\n"
    "in float vFog;\n"
    "uniform sampler2D uTexture;\n"
    "uniform int uUseTexture;\n"
    "uniform vec4 uPrimColor;\n"
    "uniform vec4 uEnvColor;\n"
    "uniform ivec4 uCombineColor;\n"  // a,b,c,d source IDs for color
    "uniform ivec4 uCombineAlpha;\n"  // a,b,c,d source IDs for alpha
    "uniform vec4 uFogColor;\n"
    "uniform int uUseFog;\n"
    "out vec4 FragColor;\n"
    "\n"
    // Simplified combine source IDs:
    // 0=ZERO, 1=TEXEL0, 2=SHADE, 3=PRIMITIVE, 4=ENVIRONMENT, 5=ONE
    // 6=TEXEL0_ALPHA, 7=SHADE_ALPHA, 8=PRIM_ALPHA, 9=ENV_ALPHA
    "vec3 getColorSrc(int id, vec3 tex, vec3 shade) {\n"
    "    if (id == 1) return tex;\n"
    "    if (id == 2) return shade;\n"
    "    if (id == 3) return uPrimColor.rgb;\n"
    "    if (id == 4) return uEnvColor.rgb;\n"
    "    if (id == 5) return vec3(1.0);\n"
    "    if (id == 6) return vec3(texture(uTexture, vTexCoord).a);\n"
    "    if (id == 7) return vec3(vColor.a);\n"
    "    if (id == 8) return vec3(uPrimColor.a);\n"
    "    if (id == 9) return vec3(uEnvColor.a);\n"
    "    return vec3(0.0);\n"
    "}\n"
    "float getAlphaSrc(int id, float texA, float shadeA) {\n"
    "    if (id == 1) return texA;\n"
    "    if (id == 2) return shadeA;\n"
    "    if (id == 3) return uPrimColor.a;\n"
    "    if (id == 4) return uEnvColor.a;\n"
    "    if (id == 5) return 1.0;\n"
    "    return 0.0;\n"
    "}\n"
    "void main() {\n"
    "    vec4 texColor = (uUseTexture != 0) ? texture(uTexture, vTexCoord) : vec4(1.0);\n"
    "    vec3 a = getColorSrc(uCombineColor.x, texColor.rgb, vColor.rgb);\n"
    "    vec3 b = getColorSrc(uCombineColor.y, texColor.rgb, vColor.rgb);\n"
    "    vec3 c = getColorSrc(uCombineColor.z, texColor.rgb, vColor.rgb);\n"
    "    vec3 d = getColorSrc(uCombineColor.w, texColor.rgb, vColor.rgb);\n"
    "    float aA = getAlphaSrc(uCombineAlpha.x, texColor.a, vColor.a);\n"
    "    float bA = getAlphaSrc(uCombineAlpha.y, texColor.a, vColor.a);\n"
    "    float cA = getAlphaSrc(uCombineAlpha.z, texColor.a, vColor.a);\n"
    "    float dA = getAlphaSrc(uCombineAlpha.w, texColor.a, vColor.a);\n"
    "    FragColor.rgb = clamp((a - b) * c + d, 0.0, 1.0);\n"
    "    FragColor.a = clamp((aA - bA) * cA + dA, 0.0, 1.0);\n"
    "    if (uUseFog != 0) {\n"
    "        float fog = clamp(vFog, 0.0, 1.0);\n"
    "        FragColor.rgb = mix(FragColor.rgb, uFogColor.rgb, fog);\n"
    "    }\n"
    "}\n";

static GLuint compile_shader(GLenum type, const char* src) {
    GLuint shader = glCreateShader(type);
    glShaderSource(shader, 1, &src, NULL);
    glCompileShader(shader);

    GLint ok;
    glGetShaderiv(shader, GL_COMPILE_STATUS, &ok);
    if (!ok) {
        char log[512];
        glGetShaderInfoLog(shader, sizeof(log), NULL, log);
        fprintf(stderr, "[RDP] Shader compile error: %s\n", log);
        glDeleteShader(shader);
        return 0;
    }
    return shader;
}

static GLuint link_program(GLuint vs, GLuint fs) {
    GLuint prog = glCreateProgram();
    glAttachShader(prog, vs);
    glAttachShader(prog, fs);
    glLinkProgram(prog);

    GLint ok;
    glGetProgramiv(prog, GL_LINK_STATUS, &ok);
    if (!ok) {
        char log[512];
        glGetProgramInfoLog(prog, sizeof(log), NULL, log);
        fprintf(stderr, "[RDP] Shader link error: %s\n", log);
        glDeleteProgram(prog);
        return 0;
    }

    glDeleteShader(vs);
    glDeleteShader(fs);
    return prog;
}

static void init_shaders(void) {
    if (sShadersReady) return;

    // -- Rectangle shader --
    {
        GLuint vs = compile_shader(GL_VERTEX_SHADER, sRectVertSrc);
        GLuint fs = compile_shader(GL_FRAGMENT_SHADER, sRectFragSrc);
        if (!vs || !fs) return;
        sRectProgram = link_program(vs, fs);
        if (!sRectProgram) return;

        sRectColorLoc = glGetUniformLocation(sRectProgram, "uColor");

        glGenVertexArrays(1, &sRectVAO);
        glGenBuffers(1, &sRectVBO);
        glBindVertexArray(sRectVAO);
        glBindBuffer(GL_ARRAY_BUFFER, sRectVBO);
        glBufferData(GL_ARRAY_BUFFER, 8 * sizeof(float), NULL, GL_DYNAMIC_DRAW);
        glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 2 * sizeof(float), (void*)0);
        glEnableVertexAttribArray(0);
        glBindVertexArray(0);
    }

    // -- Triangle shader --
    {
        GLuint vs = compile_shader(GL_VERTEX_SHADER, sTriVertSrc);
        GLuint fs = compile_shader(GL_FRAGMENT_SHADER, sTriFragSrc);
        if (!vs || !fs) return;
        sTriProgram = link_program(vs, fs);
        if (!sTriProgram) return;

        sTriUseTexLoc = glGetUniformLocation(sTriProgram, "uUseTexture");
        sTriTexSamplerLoc = glGetUniformLocation(sTriProgram, "uTexture");
        sTriPrimColorLoc = glGetUniformLocation(sTriProgram, "uPrimColor");
        sTriEnvColorLoc = glGetUniformLocation(sTriProgram, "uEnvColor");
        sTriCombColorLoc = glGetUniformLocation(sTriProgram, "uCombineColor");
        sTriCombAlphaLoc = glGetUniformLocation(sTriProgram, "uCombineAlpha");
        sTriFogColorLoc = glGetUniformLocation(sTriProgram, "uFogColor");
        sTriUseFogLoc = glGetUniformLocation(sTriProgram, "uUseFog");

        glGenVertexArrays(1, &sTriVAO);
        glGenBuffers(1, &sTriVBO);
        glBindVertexArray(sTriVAO);
        glBindBuffer(GL_ARRAY_BUFFER, sTriVBO);
        glBufferData(GL_ARRAY_BUFFER, sizeof(sTriBatch), NULL, GL_DYNAMIC_DRAW);
        // Attribute 0: position (vec4)
        glVertexAttribPointer(0, 4, GL_FLOAT, GL_FALSE, TRI_VERTEX_FLOATS * sizeof(float), (void*)0);
        glEnableVertexAttribArray(0);
        // Attribute 1: color (vec4)
        glVertexAttribPointer(1, 4, GL_FLOAT, GL_FALSE, TRI_VERTEX_FLOATS * sizeof(float), (void*)(4 * sizeof(float)));
        glEnableVertexAttribArray(1);
        // Attribute 2: texture coordinates (vec2)
        glVertexAttribPointer(2, 2, GL_FLOAT, GL_FALSE, TRI_VERTEX_FLOATS * sizeof(float), (void*)(8 * sizeof(float)));
        glEnableVertexAttribArray(2);
        // Attribute 3: fog factor (float)
        glVertexAttribPointer(3, 1, GL_FLOAT, GL_FALSE, TRI_VERTEX_FLOATS * sizeof(float), (void*)(10 * sizeof(float)));
        glEnableVertexAttribArray(3);
        glBindVertexArray(0);
    }

    sShadersReady = TRUE;
}

// ============================================================================
// Matrix operations
// ============================================================================

static void mtx_identity(f32 m[4][4]) {
    memset(m, 0, 16 * sizeof(f32));
    m[0][0] = m[1][1] = m[2][2] = m[3][3] = 1.0f;
}

// result = a * b (row-major N64 convention)
static void mtx_multiply(f32 result[4][4], f32 a[4][4], f32 b[4][4]) {
    f32 tmp[4][4];
    for (s32 i = 0; i < 4; i++) {
        for (s32 j = 0; j < 4; j++) {
            tmp[i][j] = a[i][0] * b[0][j] + a[i][1] * b[1][j]
                      + a[i][2] * b[2][j] + a[i][3] * b[3][j];
        }
    }
    memcpy(result, tmp, sizeof(tmp));
}

static void rsp_update_mvp(void) {
    if (!sRSP.mvpDirty) return;
    mtx_multiply(sRSP.mvp,
                 sRSP.modelview[sRSP.modelviewTop],
                 sRSP.projection[sRSP.projectionTop]);
    sRSP.mvpDirty = FALSE;
}

// ============================================================================
// N64 Texture Format Conversion
// ============================================================================

// Convert RGBA16 (5:5:5:1) to RGBA8
static void convert_rgba16_to_rgba8(const u8* src, u8* dst, s32 numTexels) {
    for (s32 i = 0; i < numTexels; i++) {
        u16 c = (src[i * 2] << 8) | src[i * 2 + 1]; // Big-endian
        dst[i * 4 + 0] = ((c >> 11) & 0x1F) * 255 / 31; // R
        dst[i * 4 + 1] = ((c >>  6) & 0x1F) * 255 / 31; // G
        dst[i * 4 + 2] = ((c >>  1) & 0x1F) * 255 / 31; // B
        dst[i * 4 + 3] = (c & 1) ? 255 : 0;              // A
    }
}

// Convert RGBA32 to RGBA8 (just endian swap)
static void convert_rgba32_to_rgba8(const u8* src, u8* dst, s32 numTexels) {
    for (s32 i = 0; i < numTexels; i++) {
        dst[i * 4 + 0] = src[i * 4 + 0]; // R
        dst[i * 4 + 1] = src[i * 4 + 1]; // G
        dst[i * 4 + 2] = src[i * 4 + 2]; // B
        dst[i * 4 + 3] = src[i * 4 + 3]; // A
    }
}

// Convert IA16 (I8:A8) to RGBA8
static void convert_ia16_to_rgba8(const u8* src, u8* dst, s32 numTexels) {
    for (s32 i = 0; i < numTexels; i++) {
        u8 intensity = src[i * 2];
        u8 alpha = src[i * 2 + 1];
        dst[i * 4 + 0] = intensity;
        dst[i * 4 + 1] = intensity;
        dst[i * 4 + 2] = intensity;
        dst[i * 4 + 3] = alpha;
    }
}

// Convert IA8 (I4:A4) to RGBA8
static void convert_ia8_to_rgba8(const u8* src, u8* dst, s32 numTexels) {
    for (s32 i = 0; i < numTexels; i++) {
        u8 intensity = ((src[i] >> 4) & 0xF) * 255 / 15;
        u8 alpha = (src[i] & 0xF) * 255 / 15;
        dst[i * 4 + 0] = intensity;
        dst[i * 4 + 1] = intensity;
        dst[i * 4 + 2] = intensity;
        dst[i * 4 + 3] = alpha;
    }
}

// Convert IA4 (I3:A1, packed 2 per byte) to RGBA8
static void convert_ia4_to_rgba8(const u8* src, u8* dst, s32 numTexels) {
    for (s32 i = 0; i < numTexels; i++) {
        u8 byte = src[i / 2];
        u8 nibble = (i & 1) ? (byte & 0xF) : (byte >> 4);
        u8 intensity = ((nibble >> 1) & 0x7) * 255 / 7;
        u8 alpha = (nibble & 1) ? 255 : 0;
        dst[i * 4 + 0] = intensity;
        dst[i * 4 + 1] = intensity;
        dst[i * 4 + 2] = intensity;
        dst[i * 4 + 3] = alpha;
    }
}

// Convert I8 to RGBA8
static void convert_i8_to_rgba8(const u8* src, u8* dst, s32 numTexels) {
    for (s32 i = 0; i < numTexels; i++) {
        dst[i * 4 + 0] = src[i];
        dst[i * 4 + 1] = src[i];
        dst[i * 4 + 2] = src[i];
        dst[i * 4 + 3] = src[i]; // I format: intensity used for alpha too
    }
}

// Convert I4 (packed 2 per byte) to RGBA8
static void convert_i4_to_rgba8(const u8* src, u8* dst, s32 numTexels) {
    for (s32 i = 0; i < numTexels; i++) {
        u8 byte = src[i / 2];
        u8 nibble = (i & 1) ? (byte & 0xF) : (byte >> 4);
        u8 val = nibble * 255 / 15;
        dst[i * 4 + 0] = val;
        dst[i * 4 + 1] = val;
        dst[i * 4 + 2] = val;
        dst[i * 4 + 3] = val;
    }
}

// Convert CI8 (8-bit color index) to RGBA8 using TLUT palette in TMEM
static void convert_ci8_to_rgba8(const u8* src, u8* dst, s32 numTexels,
                                 const u8* tlut, u32 palIdx) {
    // TLUT is stored at TMEM offset 256 (word 256, byte 2048) as RGBA16 entries
    const u8* pal = tlut + palIdx * 256 * 2; // palette offset
    for (s32 i = 0; i < numTexels; i++) {
        u8 idx = src[i];
        u16 c = (pal[idx * 2] << 8) | pal[idx * 2 + 1]; // Big-endian RGBA5551
        dst[i * 4 + 0] = ((c >> 11) & 0x1F) * 255 / 31;
        dst[i * 4 + 1] = ((c >>  6) & 0x1F) * 255 / 31;
        dst[i * 4 + 2] = ((c >>  1) & 0x1F) * 255 / 31;
        dst[i * 4 + 3] = (c & 1) ? 255 : 0;
    }
}

// Convert CI4 (4-bit color index, packed 2 per byte) to RGBA8 using TLUT
static void convert_ci4_to_rgba8(const u8* src, u8* dst, s32 numTexels,
                                 const u8* tlut, u32 palIdx) {
    // For CI4, palette has 16 entries at TMEM offset 256 + palIdx*16*2
    const u8* pal = tlut + palIdx * 16 * 2;
    for (s32 i = 0; i < numTexels; i++) {
        u8 byte = src[i / 2];
        u8 idx = (i & 1) ? (byte & 0xF) : (byte >> 4);
        u16 c = (pal[idx * 2] << 8) | pal[idx * 2 + 1]; // Big-endian RGBA5551
        dst[i * 4 + 0] = ((c >> 11) & 0x1F) * 255 / 31;
        dst[i * 4 + 1] = ((c >>  6) & 0x1F) * 255 / 31;
        dst[i * 4 + 2] = ((c >>  1) & 0x1F) * 255 / 31;
        dst[i * 4 + 3] = (c & 1) ? 255 : 0;
    }
}

// Convert N64 texture data in TMEM to RGBA8 based on tile descriptor.
// The key insight: TMEM stores rows with a stride of `tile->line` 64-bit words,
// which may differ from the texture's pixel width. We must read row-by-row
// using this stride, or we get diagonal stripe artifacts.
// Returns number of texels converted, or 0 on failure.
static s32 convert_tmem_to_rgba8(const TileDescriptor* tile, u8* outBuf, s32 maxBytes) {
    s32 width = (s32)((tile->lrs - tile->uls) / 4.0f) + 1;
    s32 height = (s32)((tile->lrt - tile->ult) / 4.0f) + 1;
    if (width <= 0 || height <= 0 || width > 1024 || height > 1024) return 0;

    s32 numTexels = width * height;
    if (numTexels * 4 > maxBytes) return 0;

    u32 tmemByteOfs = tile->tmem * 8; // tmem is in 64-bit word units
    if (tmemByteOfs >= TMEM_SIZE) return 0;

    // TLUT palette data is at TMEM offset 2048 (word 256)
    const u8* tlut = &sRDP.tmem[2048];

    // TMEM row stride in bytes (line field = 64-bit words per row)
    u32 tmemLineBytes = tile->line * 8;

    // Calculate the tight row size in bytes based on format
    u32 rowBytes = 0;
    switch (tile->siz) {
        case G_IM_SIZ_4b:  rowBytes = (width + 1) / 2; break;
        case G_IM_SIZ_8b:  rowBytes = width; break;
        case G_IM_SIZ_16b: rowBytes = width * 2; break;
        case G_IM_SIZ_32b: rowBytes = width * 4; break;
    }

    // If TMEM line stride matches the tight row size (or line==0),
    // data is contiguous and we can convert directly.
    // Otherwise, we need to deinterleave row by row first.
    s32 needsDeinterleave = (tmemLineBytes != 0 && tmemLineBytes != rowBytes);

    // Temporary buffer for row-deinterleaved data
    static u8 sRowBuf[TMEM_SIZE];
    const u8* src;

    if (needsDeinterleave && tmemLineBytes > 0) {
        // Copy each row from TMEM using the line stride into a contiguous buffer
        for (s32 row = 0; row < height; row++) {
            u32 srcOfs = tmemByteOfs + row * tmemLineBytes;
            u32 dstOfs = row * rowBytes;
            if (srcOfs + rowBytes > TMEM_SIZE) break;
            if (dstOfs + rowBytes > TMEM_SIZE) break;
            memcpy(&sRowBuf[dstOfs], &sRDP.tmem[srcOfs], rowBytes);
        }
        src = sRowBuf;
    } else {
        src = &sRDP.tmem[tmemByteOfs];
    }

    switch (tile->fmt) {
        case G_IM_FMT_RGBA:
            switch (tile->siz) {
                case G_IM_SIZ_16b: convert_rgba16_to_rgba8(src, outBuf, numTexels); break;
                case G_IM_SIZ_32b: convert_rgba32_to_rgba8(src, outBuf, numTexels); break;
                default: memset(outBuf, 0xFF, numTexels * 4); break;
            }
            break;
        case G_IM_FMT_IA:
            switch (tile->siz) {
                case G_IM_SIZ_16b: convert_ia16_to_rgba8(src, outBuf, numTexels); break;
                case G_IM_SIZ_8b:  convert_ia8_to_rgba8(src, outBuf, numTexels); break;
                case G_IM_SIZ_4b:  convert_ia4_to_rgba8(src, outBuf, numTexels); break;
                default: memset(outBuf, 0xFF, numTexels * 4); break;
            }
            break;
        case G_IM_FMT_I:
            switch (tile->siz) {
                case G_IM_SIZ_8b: convert_i8_to_rgba8(src, outBuf, numTexels); break;
                case G_IM_SIZ_4b: convert_i4_to_rgba8(src, outBuf, numTexels); break;
                default: memset(outBuf, 0xFF, numTexels * 4); break;
            }
            break;
        case G_IM_FMT_CI:
            switch (tile->siz) {
                case G_IM_SIZ_8b: convert_ci8_to_rgba8(src, outBuf, numTexels, tlut, tile->palette); break;
                case G_IM_SIZ_4b: convert_ci4_to_rgba8(src, outBuf, numTexels, tlut, tile->palette); break;
                default: memset(outBuf, 0xFF, numTexels * 4); break;
            }
            break;
        default:
            memset(outBuf, 0xFF, numTexels * 4);
            break;
    }

    return numTexels;
}

// Upload texture data for a tile to OpenGL
static void upload_tile_texture(u32 tileIdx) {
    static s32 sFirstUploadReported = FALSE;
    if (tileIdx >= N64_TILE_COUNT) return;
    TileDescriptor* tile = &sRDP.tiles[tileIdx];
    if (!tile->sizeSet) return;

    s32 width = (s32)((tile->lrs - tile->uls) / 4.0f) + 1;
    s32 height = (s32)((tile->lrt - tile->ult) / 4.0f) + 1;
    if (width <= 0 || height <= 0 || width > 1024 || height > 1024) return;

    // Convert TMEM data to RGBA8
    s32 numTexels = convert_tmem_to_rgba8(tile, sTexConvBuf, TEX_CONV_BUF_SIZE);
    if (numTexels == 0) return;

    // Check if texture data is all zeros (from DMA stub - ROM data not loaded)
    // If so, skip upload so triangles fall back to vertex color rendering
    s32 hasData = FALSE;
    for (s32 j = 0; j < numTexels * 4; j += 4) {
        if (sTexConvBuf[j] || sTexConvBuf[j+1] || sTexConvBuf[j+2] || sTexConvBuf[j+3]) {
            hasData = TRUE;
            break;
        }
    }
    if (!hasData) return;

    if (!sFirstUploadReported) {
        fprintf(stderr, "[RDP] First non-empty texture: tile=%u %dx%d fmt=%u siz=%u\n",
                tileIdx, width, height, tile->fmt, tile->siz);
        sFirstUploadReported = TRUE;
    }

    // Create or update OpenGL texture
    if (sTileTextures[tileIdx] == 0) {
        glGenTextures(1, &sTileTextures[tileIdx]);
    }

    glBindTexture(GL_TEXTURE_2D, sTileTextures[tileIdx]);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, width, height, 0,
                 GL_RGBA, GL_UNSIGNED_BYTE, sTexConvBuf);

    // Set filtering and wrapping based on tile descriptor
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);

    // S-axis wrapping
    if (tile->cms & G_TX_CLAMP) {
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    } else if (tile->cms & G_TX_MIRROR) {
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_MIRRORED_REPEAT);
    } else {
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
    }

    // T-axis wrapping
    if (tile->cmt & G_TX_CLAMP) {
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    } else if (tile->cmt & G_TX_MIRROR) {
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_MIRRORED_REPEAT);
    } else {
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
    }

    glBindTexture(GL_TEXTURE_2D, 0);

    sTileTexWidth[tileIdx] = width;
    sTileTexHeight[tileIdx] = height;
    sTileTexGeneration[tileIdx] = sTmemGeneration;
}

// ============================================================================
// Combiner mode decode
// ============================================================================

// Simplified source IDs for the shader
#define COMB_ZERO        0
#define COMB_TEXEL0      1
#define COMB_SHADE       2
#define COMB_PRIMITIVE   3
#define COMB_ENVIRONMENT 4
#define COMB_ONE         5
#define COMB_TEX0_ALPHA  6
#define COMB_SHADE_ALPHA 7
#define COMB_PRIM_ALPHA  8
#define COMB_ENV_ALPHA   9

// Map GBI CC_MUX values for A/B inputs (4-bit) to simplified IDs
static s32 map_cc_ab(u32 mux) {
    switch (mux) {
        case 1: return COMB_TEXEL0;      // G_CCMUX_TEXEL0
        case 3: return COMB_PRIMITIVE;    // G_CCMUX_PRIMITIVE
        case 4: return COMB_SHADE;        // G_CCMUX_SHADE
        case 5: return COMB_ENVIRONMENT;  // G_CCMUX_ENVIRONMENT
        case 6: return COMB_ONE;          // G_CCMUX_1 (for A only)
        default: return COMB_ZERO;        // COMBINED, TEXEL1, NOISE, etc.
    }
}

// Map GBI CC_MUX values for C input (5-bit, includes alpha sources)
static s32 map_cc_c(u32 mux) {
    switch (mux) {
        case 1:  return COMB_TEXEL0;      // G_CCMUX_TEXEL0
        case 3:  return COMB_PRIMITIVE;    // G_CCMUX_PRIMITIVE
        case 4:  return COMB_SHADE;        // G_CCMUX_SHADE
        case 5:  return COMB_ENVIRONMENT;  // G_CCMUX_ENVIRONMENT
        case 8:  return COMB_TEX0_ALPHA;   // G_CCMUX_TEXEL0_ALPHA
        case 10: return COMB_PRIM_ALPHA;   // G_CCMUX_PRIMITIVE_ALPHA
        case 11: return COMB_SHADE_ALPHA;  // G_CCMUX_SHADE_ALPHA
        case 12: return COMB_ENV_ALPHA;    // G_CCMUX_ENV_ALPHA
        default: return COMB_ZERO;         // COMBINED, TEXEL1, LOD, etc.
    }
}

// Map GBI CC_MUX values for D input (3-bit)
static s32 map_cc_d(u32 mux) {
    switch (mux) {
        case 1: return COMB_TEXEL0;      // G_CCMUX_TEXEL0
        case 3: return COMB_PRIMITIVE;    // G_CCMUX_PRIMITIVE
        case 4: return COMB_SHADE;        // G_CCMUX_SHADE
        case 5: return COMB_ENVIRONMENT;  // G_CCMUX_ENVIRONMENT
        case 6: return COMB_ONE;          // G_CCMUX_1
        default: return COMB_ZERO;        // COMBINED, TEXEL1, 0
    }
}

// Map GBI AC_MUX values for A/B/D inputs (3-bit)
static s32 map_ac_abd(u32 mux) {
    switch (mux) {
        case 1: return COMB_TEXEL0;      // G_ACMUX_TEXEL0
        case 3: return COMB_PRIMITIVE;    // G_ACMUX_PRIMITIVE
        case 4: return COMB_SHADE;        // G_ACMUX_SHADE
        case 5: return COMB_ENVIRONMENT;  // G_ACMUX_ENVIRONMENT
        case 6: return COMB_ONE;          // G_ACMUX_1
        default: return COMB_ZERO;        // COMBINED, TEXEL1, 0
    }
}

// Map GBI AC_MUX values for C input (3-bit, different table)
static s32 map_ac_c(u32 mux) {
    switch (mux) {
        case 1: return COMB_TEXEL0;      // G_ACMUX_TEXEL0
        case 3: return COMB_PRIMITIVE;    // G_ACMUX_PRIMITIVE
        case 4: return COMB_SHADE;        // G_ACMUX_SHADE
        case 5: return COMB_ENVIRONMENT;  // G_ACMUX_ENVIRONMENT
        default: return COMB_ZERO;        // LOD_FRACTION, TEXEL1, PRIM_LOD, 0
    }
}

// Decode combineHi/Lo into simplified source IDs for cycle 0
static void decode_combine_mode(u32 combHi, u32 combLo, s32 colorOut[4], s32 alphaOut[4]) {
    // Cycle 0 color: (a - b) * c + d
    colorOut[0] = map_cc_ab((combHi >> 20) & 0xF);  // a
    colorOut[1] = map_cc_ab((combLo >> 28) & 0xF);  // b
    colorOut[2] = map_cc_c((combHi >> 15) & 0x1F);  // c
    colorOut[3] = map_cc_d((combLo >> 15) & 0x7);   // d

    // Cycle 0 alpha: (a - b) * c + d
    alphaOut[0] = map_ac_abd((combHi >> 12) & 0x7);  // a
    alphaOut[1] = map_ac_abd((combLo >> 12) & 0x7);  // b
    alphaOut[2] = map_ac_c((combHi >> 9) & 0x7);     // c
    alphaOut[3] = map_ac_abd((combLo >> 9) & 0x7);   // d
}

// ============================================================================
// Triangle batching
// ============================================================================

static void flush_triangles(void) {
    if (sTriBatchCount == 0) return;

    glUseProgram(sTriProgram);

    // --- Depth testing from otherModeL ---
    u32 mode = sTriBatchOtherModeL;
    s32 zCmp = (mode & 0x0010) != 0;  // Z_CMP
    s32 zUpd = (mode & 0x0020) != 0;  // Z_UPD
    u32 zMode = (mode >> 10) & 0x3;   // ZMODE (0=OPA, 1=INTER, 2=XLU, 3=DEC)

    if (zCmp) {
        glEnable(GL_DEPTH_TEST);
        glDepthFunc(GL_LEQUAL);
    } else {
        glDisable(GL_DEPTH_TEST);
    }
    glDepthMask(zUpd ? GL_TRUE : GL_FALSE);

    // Decal mode: use polygon offset to avoid z-fighting
    if (zMode == 3) {
        glEnable(GL_POLYGON_OFFSET_FILL);
        glPolygonOffset(-1.0f, -1.0f);
    } else {
        glDisable(GL_POLYGON_OFFSET_FILL);
    }

    // --- Alpha blending ---
    s32 forceBl = (mode & 0x4000) != 0;  // FORCE_BL
    s32 zModeXlu = (zMode == 2);         // ZMODE_XLU

    if (forceBl || zModeXlu) {
        glEnable(GL_BLEND);
        glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    } else {
        // Opaque: still need alpha test behavior (discard fully transparent)
        glEnable(GL_BLEND);
        glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    }

    // --- Fog ---
    s32 useFog = (sRSP.geometryMode & G_FOG) != 0;
    if (useFog) {
        glUniform4f(sTriFogColorLoc, sRSP.fogR, sRSP.fogG, sRSP.fogB, 1.0f);
        glUniform1i(sTriUseFogLoc, 1);
    } else {
        glUniform1i(sTriUseFogLoc, 0);
    }

    // Set texture uniforms
    if (sTriBatchTextured && sTriBatchTexId != 0) {
        glActiveTexture(GL_TEXTURE0);
        glBindTexture(GL_TEXTURE_2D, sTriBatchTexId);
        glUniform1i(sTriTexSamplerLoc, 0);
        glUniform1i(sTriUseTexLoc, 1);
    } else {
        glUniform1i(sTriUseTexLoc, 0);
    }

    // Set combiner uniforms
    glUniform4f(sTriPrimColorLoc,
                sTriBatchPrimColor[0], sTriBatchPrimColor[1],
                sTriBatchPrimColor[2], sTriBatchPrimColor[3]);
    glUniform4f(sTriEnvColorLoc,
                sTriBatchEnvColor[0], sTriBatchEnvColor[1],
                sTriBatchEnvColor[2], sTriBatchEnvColor[3]);
    glUniform4i(sTriCombColorLoc,
                sTriBatchCombColor[0], sTriBatchCombColor[1],
                sTriBatchCombColor[2], sTriBatchCombColor[3]);
    glUniform4i(sTriCombAlphaLoc,
                sTriBatchCombAlpha[0], sTriBatchCombAlpha[1],
                sTriBatchCombAlpha[2], sTriBatchCombAlpha[3]);

    glBindVertexArray(sTriVAO);
    glBindBuffer(GL_ARRAY_BUFFER, sTriVBO);
    s32 dataSize = sTriBatchCount * 3 * TRI_VERTEX_FLOATS * sizeof(f32);
    glBufferSubData(GL_ARRAY_BUFFER, 0, dataSize, sTriBatch);
    glDrawArrays(GL_TRIANGLES, 0, sTriBatchCount * 3);
    glBindVertexArray(0);

    // Unbind texture
    if (sTriBatchTextured && sTriBatchTexId != 0) {
        glBindTexture(GL_TEXTURE_2D, 0);
    }

    // Restore GL state
    glDisable(GL_BLEND);
    glDisable(GL_DEPTH_TEST);
    glDepthMask(GL_TRUE);
    glDisable(GL_POLYGON_OFFSET_FILL);

    sTriBatchCount = 0;
    sTriBatchTextured = FALSE;
    sTriBatchTexId = 0;
}

static s32 sFrameTotalTris = 0;
static s32 sFrameOnScreenTris = 0;

// Check if a triangle has any vertex potentially on screen
static s32 tri_potentially_visible(RSPVertex* v0, RSPVertex* v1, RSPVertex* v2) {
    // Check if all 3 vertices are outside the same clip plane
    // If so, the triangle is definitely off-screen
    RSPVertex* verts[3] = { v0, v1, v2 };
    for (s32 i = 0; i < 3; i++) {
        if (verts[i]->pos[3] <= 0.0f) return 0; // Behind camera
    }

    f32 ndc[3][3];
    for (s32 i = 0; i < 3; i++) {
        f32 w = verts[i]->pos[3];
        ndc[i][0] = verts[i]->pos[0] / w;
        ndc[i][1] = verts[i]->pos[1] / w;
        ndc[i][2] = verts[i]->pos[2] / w;
    }

    // Check each clip plane: all 3 vertices on same side = trivially rejected
    if (ndc[0][0] < -1 && ndc[1][0] < -1 && ndc[2][0] < -1) return 0; // all left
    if (ndc[0][0] >  1 && ndc[1][0] >  1 && ndc[2][0] >  1) return 0; // all right
    if (ndc[0][1] < -1 && ndc[1][1] < -1 && ndc[2][1] < -1) return 0; // all below
    if (ndc[0][1] >  1 && ndc[1][1] >  1 && ndc[2][1] >  1) return 0; // all above
    if (ndc[0][2] < -1 && ndc[1][2] < -1 && ndc[2][2] < -1) return 0; // all behind near
    if (ndc[0][2] >  1 && ndc[1][2] >  1 && ndc[2][2] >  1) return 0; // all beyond far
    return 1;
}

static void emit_triangle(s32 v0, s32 v1, s32 v2) {
    // Validate indices
    if (v0 < 0 || v0 >= RSP_MAX_VERTICES ||
        v1 < 0 || v1 >= RSP_MAX_VERTICES ||
        v2 < 0 || v2 >= RSP_MAX_VERTICES) {
        return;
    }

    // Determine current texture state
    s32 useTexture = FALSE;
    GLuint texId = 0;
    f32 tileWidth = 1.0f, tileHeight = 1.0f;

    if (sRSP.texEnabled) {
        u32 tIdx = sRSP.texTile;
        if (tIdx < N64_TILE_COUNT && sRDP.tiles[tIdx].sizeSet) {
            TileDescriptor* tile = &sRDP.tiles[tIdx];
            s32 tw = (s32)((tile->lrs - tile->uls) / 4.0f) + 1;
            s32 th = (s32)((tile->lrt - tile->ult) / 4.0f) + 1;
            if (tw > 0 && th > 0) {
                // Upload texture if needed
                if (sTileTexGeneration[tIdx] != sTmemGeneration || sTileTextures[tIdx] == 0) {
                    upload_tile_texture(tIdx);
                }
                if (sTileTextures[tIdx] != 0) {
                    useTexture = TRUE;
                    texId = sTileTextures[tIdx];
                    tileWidth = (f32)tw;
                    tileHeight = (f32)th;
                }
            }
        }
    }

    // Decode current combine mode
    s32 curCombColor[4], curCombAlpha[4];
    decode_combine_mode(sRDP.combineHi, sRDP.combineLo, curCombColor, curCombAlpha);

    // If texture or combiner state changed, flush
    s32 combChanged = FALSE;
    if (sTriBatchCount > 0) {
        if (useTexture != sTriBatchTextured || texId != sTriBatchTexId) {
            combChanged = TRUE;
        }
        if (memcmp(curCombColor, sTriBatchCombColor, sizeof(curCombColor)) != 0 ||
            memcmp(curCombAlpha, sTriBatchCombAlpha, sizeof(curCombAlpha)) != 0) {
            combChanged = TRUE;
        }
        // Check if prim/env colors changed
        if (sRDP.primR != sTriBatchPrimColor[0] || sRDP.primG != sTriBatchPrimColor[1] ||
            sRDP.primB != sTriBatchPrimColor[2] || sRDP.primA != sTriBatchPrimColor[3] ||
            sRDP.envR != sTriBatchEnvColor[0] || sRDP.envG != sTriBatchEnvColor[1] ||
            sRDP.envB != sTriBatchEnvColor[2] || sRDP.envA != sTriBatchEnvColor[3]) {
            combChanged = TRUE;
        }
        // Check if render mode changed (affects depth/blend GL state)
        if (sRDP.otherModeL != sTriBatchOtherModeL) {
            combChanged = TRUE;
        }
        if (combChanged) {
            flush_triangles();
        }
    }

    if (sTriBatchCount >= TRI_BATCH_MAX) {
        flush_triangles();
    }

    sTriBatchTextured = useTexture;
    sTriBatchTexId = texId;
    memcpy(sTriBatchCombColor, curCombColor, sizeof(curCombColor));
    memcpy(sTriBatchCombAlpha, curCombAlpha, sizeof(curCombAlpha));
    sTriBatchPrimColor[0] = sRDP.primR; sTriBatchPrimColor[1] = sRDP.primG;
    sTriBatchPrimColor[2] = sRDP.primB; sTriBatchPrimColor[3] = sRDP.primA;
    sTriBatchEnvColor[0] = sRDP.envR; sTriBatchEnvColor[1] = sRDP.envG;
    sTriBatchEnvColor[2] = sRDP.envB; sTriBatchEnvColor[3] = sRDP.envA;
    sTriBatchOtherModeL = sRDP.otherModeL;

    RSPVertex* verts[3] = { &sRSP.vtxBuf[v0], &sRSP.vtxBuf[v1], &sRSP.vtxBuf[v2] };

    sFrameTotalTris++;
    s32 visible = tri_potentially_visible(verts[0], verts[1], verts[2]);
    if (visible) sFrameOnScreenTris++;

    f32* dst = &sTriBatch[sTriBatchCount * 3 * TRI_VERTEX_FLOATS];
    for (s32 i = 0; i < 3; i++) {
        RSPVertex* v = verts[i];
        // Position (clip space)
        *dst++ = v->pos[0];
        *dst++ = v->pos[1];
        *dst++ = v->pos[2];
        *dst++ = v->pos[3];
        // Color
        *dst++ = v->color[0];
        *dst++ = v->color[1];
        *dst++ = v->color[2];
        *dst++ = v->color[3];
        // Texture coordinates (normalized to 0-1)
        if (useTexture) {
            *dst++ = v->tc[0] / tileWidth;
            *dst++ = v->tc[1] / tileHeight;
        } else {
            *dst++ = 0.0f;
            *dst++ = 0.0f;
        }
        // Fog factor
        *dst++ = v->fog;
    }
    sTriBatchCount++;
}

// ============================================================================
// RSP command implementations
// ============================================================================

// Transform vertices through the MVP matrix and store in vertex buffer
static void rsp_load_vertices(Vtx* vtxAddr, s32 numVerts, s32 startIndex) {
    if (!vtxAddr) return;

    rsp_update_mvp();

    for (s32 i = 0; i < numVerts; i++) {
        s32 idx = startIndex + i;
        if (idx < 0 || idx >= RSP_MAX_VERTICES) continue;

        Vtx_t* src = &vtxAddr[i].v;
        RSPVertex* dst = &sRSP.vtxBuf[idx];

        // Object-space position
        f32 x = (f32)src->ob[0];
        f32 y = (f32)src->ob[1];
        f32 z = (f32)src->ob[2];

        // Transform by MVP matrix (row-major: pos * matrix)
        dst->pos[0] = x * sRSP.mvp[0][0] + y * sRSP.mvp[1][0] + z * sRSP.mvp[2][0] + sRSP.mvp[3][0];
        dst->pos[1] = x * sRSP.mvp[0][1] + y * sRSP.mvp[1][1] + z * sRSP.mvp[2][1] + sRSP.mvp[3][1];
        dst->pos[2] = x * sRSP.mvp[0][2] + y * sRSP.mvp[1][2] + z * sRSP.mvp[2][2] + sRSP.mvp[3][2];
        dst->pos[3] = x * sRSP.mvp[0][3] + y * sRSP.mvp[1][3] + z * sRSP.mvp[2][3] + sRSP.mvp[3][3];

        // Vertex color (RGBA, 0-255 -> 0-1)
        dst->color[0] = src->cn[0] / 255.0f;
        dst->color[1] = src->cn[1] / 255.0f;
        dst->color[2] = src->cn[2] / 255.0f;
        dst->color[3] = src->cn[3] / 255.0f;

        // Texture coords (s10.5 fixed point → texel coords)
        // RSP applies G_TEXTURE scale: tc * scale / 65536
        dst->tc[0] = (f32)src->tc[0] / 32.0f;
        dst->tc[1] = (f32)src->tc[1] / 32.0f;
        if (sRSP.texEnabled && sRSP.texScaleS != 0) {
            dst->tc[0] *= sRSP.texScaleS / 65536.0f;
        }
        if (sRSP.texEnabled && sRSP.texScaleT != 0) {
            dst->tc[1] *= sRSP.texScaleT / 65536.0f;
        }

        // Fog: computed from clip-space W (eye-space Z)
        // N64 RSP formula: fog = clamp(z * fogMul + fogOfs, 0, 255) / 255
        // where z is the clip-space W mapped to 0-1 range
        if (sRSP.geometryMode & G_FOG) {
            f32 w = dst->pos[3];
            if (w != 0.0f) {
                // z in 0..1 range (normalized depth)
                f32 zNorm = dst->pos[2] / w * 0.5f + 0.5f;
                f32 fogVal = zNorm * (f32)sRSP.fogMul + (f32)sRSP.fogOfs;
                fogVal = fogVal / 255.0f;
                if (fogVal < 0.0f) fogVal = 0.0f;
                if (fogVal > 1.0f) fogVal = 1.0f;
                dst->fog = fogVal;
            } else {
                dst->fog = 0.0f;
            }
        } else {
            dst->fog = 0.0f;
        }
    }
}

static void rsp_load_matrix(Mtx* mtxAddr, u32 params) {
    if (!mtxAddr) return;

    // Convert N64 fixed-point matrix to float
    f32 mf[4][4];
    guMtxL2F(mf, mtxAddr);

    // F3DEX2: params has bits XOR'd with G_MTX_PUSH
    // Bit 2 (0x04) = projection (vs modelview)
    // Bit 1 (0x02) = load (vs multiply)
    // Bit 0 (0x01) = push (vs nopush) -- note: XOR'd with G_MTX_PUSH in macro
    s32 isProjection = (params & G_MTX_PROJECTION) != 0;
    s32 isLoad = (params & G_MTX_LOAD) != 0;
    s32 isPush = (params & G_MTX_PUSH) != 0;

    if (isProjection) {
        if (isPush && sRSP.projectionTop < RSP_MTX_STACK_SIZE - 1) {
            // Copy current to next slot, then increment
            memcpy(sRSP.projection[sRSP.projectionTop + 1],
                   sRSP.projection[sRSP.projectionTop], 16 * sizeof(f32));
            sRSP.projectionTop++;
        }
        if (isLoad) {
            memcpy(sRSP.projection[sRSP.projectionTop], mf, 16 * sizeof(f32));
        } else {
            // Multiply: current = mf * current
            f32 tmp[4][4];
            mtx_multiply(tmp, mf, sRSP.projection[sRSP.projectionTop]);
            memcpy(sRSP.projection[sRSP.projectionTop], tmp, 16 * sizeof(f32));
        }
    } else {
        if (isPush && sRSP.modelviewTop < RSP_MTX_STACK_SIZE - 1) {
            memcpy(sRSP.modelview[sRSP.modelviewTop + 1],
                   sRSP.modelview[sRSP.modelviewTop], 16 * sizeof(f32));
            sRSP.modelviewTop++;
        }
        if (isLoad) {
            memcpy(sRSP.modelview[sRSP.modelviewTop], mf, 16 * sizeof(f32));
        } else {
            f32 tmp[4][4];
            mtx_multiply(tmp, mf, sRSP.modelview[sRSP.modelviewTop]);
            memcpy(sRSP.modelview[sRSP.modelviewTop], tmp, 16 * sizeof(f32));
        }
    }

    sRSP.mvpDirty = TRUE;
}

static void rsp_pop_matrix(u32 count) {
    for (u32 i = 0; i < count; i++) {
        if (sRSP.modelviewTop > 0) {
            sRSP.modelviewTop--;
        }
    }
    sRSP.mvpDirty = TRUE;
}

static void rsp_set_viewport(Vp* vpAddr) {
    if (!vpAddr) return;

    // Viewport scale and translate are in s13.2 fixed point
    sRSP.vpScaleX = vpAddr->vp.vscale[0] / 4.0f;
    sRSP.vpScaleY = vpAddr->vp.vscale[1] / 4.0f;
    sRSP.vpScaleZ = vpAddr->vp.vscale[2] / 4.0f;
    sRSP.vpTransX = vpAddr->vp.vtrans[0] / 4.0f;
    sRSP.vpTransY = vpAddr->vp.vtrans[1] / 4.0f;
    sRSP.vpTransZ = vpAddr->vp.vtrans[2] / 4.0f;
}

// ============================================================================
// Draw helpers
// ============================================================================

// Draw a solid-color rectangle in screen coordinates (0-320 x 0-240)
static void draw_fill_rect(s32 ulx, s32 uly, s32 lrx, s32 lry, f32 r, f32 g, f32 b, f32 a) {
    if (!sShadersReady) return;

    // Flush any pending triangles before switching shader
    flush_triangles();

    // Convert N64 screen coords (0-319, 0-239) to OpenGL NDC (-1 to +1)
    // N64: (0,0) = top-left, (319,239) = bottom-right
    // OpenGL: (-1,-1) = bottom-left, (1,1) = top-right
    f32 x0 = (f32)ulx / 160.0f - 1.0f;
    f32 y0 = 1.0f - (f32)uly / 120.0f;
    f32 x1 = (f32)(lrx + 1) / 160.0f - 1.0f;
    f32 y1 = 1.0f - (f32)(lry + 1) / 120.0f;

    float verts[] = {
        x0, y1,  // bottom-left
        x1, y1,  // bottom-right
        x0, y0,  // top-left
        x1, y0,  // top-right
    };

    // Fill rects are 2D - disable depth so they don't occlude 3D geometry
    glDisable(GL_DEPTH_TEST);
    glDepthMask(GL_FALSE);

    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

    glUseProgram(sRectProgram);
    glUniform4f(sRectColorLoc, r, g, b, a);

    glBindVertexArray(sRectVAO);
    glBindBuffer(GL_ARRAY_BUFFER, sRectVBO);
    glBufferSubData(GL_ARRAY_BUFFER, 0, sizeof(verts), verts);
    glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);
    glBindVertexArray(0);

    glDisable(GL_BLEND);
    glDepthMask(GL_TRUE);
}

// ============================================================================
// Display List Walker
// ============================================================================

// Extract fill color from the 32-bit packed value
// In 16-bit color mode, fillColor has two copies of RGBA5551
static void decode_fill_color(u32 packed) {
    sRDP.fillColor = packed;
    // Extract from upper 16 bits (RGBA 5:5:5:1)
    u16 c = (u16)(packed >> 16);
    sRDP.fillR = ((c >> 11) & 0x1F) / 31.0f;
    sRDP.fillG = ((c >>  6) & 0x1F) / 31.0f;
    sRDP.fillB = ((c >>  1) & 0x1F) / 31.0f;
    sRDP.fillA = (c & 1) ? 1.0f : 0.0f;
}

// Process a single display list, following branches
static void rdp_process_dl(Gfx* dl, s32 maxCommands) {
    for (s32 i = 0; i < maxCommands; i++) {
        // Extract opcode from upper 8 bits of w0
        u32 w0 = (u32)dl[i].words.w0;
        u32 w1 = (u32)dl[i].words.w1;
        u8 opcode = (w0 >> 24) & 0xFF;

        switch (opcode) {
            case G_ENDDL:
                flush_triangles();
                return;

            case G_RDPPIPESYNC:
            case G_RDPFULLSYNC:
            case G_RDPLOADSYNC:
            case G_RDPTILESYNC:
                // Sync commands are no-ops on PC
                break;

            case G_DL: {
                // Branch to sub-display list
                flush_triangles();
                Gfx* subDL = (Gfx*)(uintptr_t)dl[i].words.w1;
                u8 push = (w0 >> 16) & 0xFF;
                if (subDL) {
                    rdp_process_dl(subDL, 4096);
                }
                if (push == G_DL_NOPUSH) {
                    return; // G_DL_NOPUSH = branch (don't return)
                }
                break;
            }

            // =============================================================
            // RSP Geometry commands
            // =============================================================

            case G_VTX: {
                // F3DEX2: w0 = [01 | v0*2(8) | n<<10|sizeof(Vtx)*n-1(16)]
                //         w1 = address of vertex data
                Vtx* vtxAddr = (Vtx*)(uintptr_t)dl[i].words.w1;
                s32 numVerts = ((w0 >> 12) & 0xFF);  // n is in bits 19:12 (after >>10 gives n, but encoded as n<<10 in bits 11:0 combined with len)
                // Actually F3DEX2 encoding: w0[15:12] = n, w0[11:1] = sizeof(Vtx)*n - 1
                // More precisely: bits [19:12] contain v0*2, bits [11:0] contain (n<<10)|(sizeof(Vtx)*n-1)
                // Let me decode properly:
                // w0 = 0x01 | (v0*2 << 16) | ((n << 10) | (sizeof(Vtx)*n - 1))
                // So: v0 = (w0 >> 16) & 0xFF, divided by 2
                // n = (w0 >> 10) & 0x3F (but this overlaps with length...)
                // Actually from the macro:
                // gSPVertex(pkt, v, n, v0) -> gDma1p(pkt, G_VTX, v, ((n)<<10)|(sizeof(Vtx)*(n)-1), (v0)*2)
                // gDma1p packs: w0 = _SHIFTL(c,24,8) | _SHIFTL(p,16,8) | _SHIFTL(l,0,16)
                // So: w0[23:16] = v0*2, w0[15:0] = ((n<<10) | (sizeof(Vtx)*n - 1))
                // Extracting n: take bits [15:0], shift right by 10 = n (in the upper 6 bits of the 16-bit field)
                // But sizeof(Vtx)*n-1 can be up to 32*16-1=511, which is 9 bits, so it fits in bits [9:0]
                s32 v0 = ((w0 >> 16) & 0xFF) / 2;
                numVerts = (w0 & 0xFFFF) >> 10;
                if (numVerts == 0) numVerts = 1;

                rsp_load_vertices(vtxAddr, numVerts, v0);
                break;
            }

            case G_MTX: {
                // F3DEX2: gDma2p encoding
                // w0 = [DA | xxx | params_byte]
                // w1 = address of Mtx
                Mtx* mtxAddr = (Mtx*)(uintptr_t)dl[i].words.w1;
                // gDma2p: w0 = _SHIFTL(c,24,8) | _SHIFTL((len-1)/8,19,5) | _SHIFTL(ofs/8,8,8) | _SHIFTL(idx,0,8)
                // For G_MTX: gDma2p(pkt, G_MTX, m, sizeof(Mtx), (p)^G_MTX_PUSH, 0)
                // idx = (p)^G_MTX_PUSH, ofs = 0
                // So params = w0 & 0xFF, but XOR'd with G_MTX_PUSH (0x01)
                u32 params = (w0 & 0xFF) ^ G_MTX_PUSH;
                flush_triangles();
                rsp_load_matrix(mtxAddr, params);
                break;
            }

            case G_POPMTX: {
                // F3DEX2: w1 = num * 64 (number of matrices to pop * sizeof(Mtx))
                u32 numPop = w1 / 64;
                if (numPop == 0) numPop = 1;
                flush_triangles();
                rsp_pop_matrix(numPop);
                break;
            }

            case G_TRI1: {
                // F3DEX2: w0 = [05 | v0*2(8) | v1*2(8) | v2*2(8)]
                s32 v0 = ((w0 >> 16) & 0xFF) / 2;
                s32 v1 = ((w0 >>  8) & 0xFF) / 2;
                s32 v2 = ((w0 >>  0) & 0xFF) / 2;
                emit_triangle(v0, v1, v2);
                break;
            }

            case G_TRI2: {
                // F3DEX2: w0 = [06 | v0*2(8) | v1*2(8) | v2*2(8)]
                //         w1 = [v3*2(8) | v4*2(8) | v5*2(8) | 0(8)]
                s32 v0 = ((w0 >> 16) & 0xFF) / 2;
                s32 v1 = ((w0 >>  8) & 0xFF) / 2;
                s32 v2 = ((w0 >>  0) & 0xFF) / 2;
                s32 v3 = ((w1 >> 16) & 0xFF) / 2;
                s32 v4 = ((w1 >>  8) & 0xFF) / 2;
                s32 v5 = ((w1 >>  0) & 0xFF) / 2;
                emit_triangle(v0, v1, v2);
                emit_triangle(v3, v4, v5);
                break;
            }

            case G_GEOMETRYMODE: {
                // F3DEX2: w0[23:0] = bits to clear (inverted), w1 = bits to set
                u32 clearBits = w0 & 0x00FFFFFF;
                u32 setBits = w1;
                sRSP.geometryMode = (sRSP.geometryMode & clearBits) | setBits;
                break;
            }

            case G_MOVEMEM: {
                // F3DEX2: w0 = [DC | (len-1)/8(5) | ofs/8(8) | idx(8)]
                // Used for viewport, lights, etc.
                u8 idx = w0 & 0xFF;
                if (idx == G_MV_VIEWPORT) {
                    Vp* vpAddr = (Vp*)(uintptr_t)dl[i].words.w1;
                    rsp_set_viewport(vpAddr);
                }
                break;
            }

            // =============================================================
            // RDP Color/mode commands
            // =============================================================

            case G_SETFILLCOLOR:
                decode_fill_color(w1);
                break;

            case G_SETPRIMCOLOR:
                // w1: [R(8) | G(8) | B(8) | A(8)]
                sRDP.primR = ((w1 >> 24) & 0xFF) / 255.0f;
                sRDP.primG = ((w1 >> 16) & 0xFF) / 255.0f;
                sRDP.primB = ((w1 >>  8) & 0xFF) / 255.0f;
                sRDP.primA = ((w1 >>  0) & 0xFF) / 255.0f;
                break;

            case G_SETENVCOLOR:
                // w1: [R(8) | G(8) | B(8) | A(8)]
                sRDP.envR = ((w1 >> 24) & 0xFF) / 255.0f;
                sRDP.envG = ((w1 >> 16) & 0xFF) / 255.0f;
                sRDP.envB = ((w1 >>  8) & 0xFF) / 255.0f;
                sRDP.envA = ((w1 >>  0) & 0xFF) / 255.0f;
                break;

            case G_SETFOGCOLOR:
                sRSP.fogR = ((w1 >> 24) & 0xFF) / 255.0f;
                sRSP.fogG = ((w1 >> 16) & 0xFF) / 255.0f;
                sRSP.fogB = ((w1 >>  8) & 0xFF) / 255.0f;
                sRSP.fogA = ((w1 >>  0) & 0xFF) / 255.0f;
                break;

            case G_FILLRECT: {
                // gDPFillRectangle encoding:
                // w0: [opcode(8) | lrx(10) | lry(10) | pad(4)]
                // w1: [0(8) | ulx(10) | uly(10) | pad(4)]
                s32 lrx = (w0 >> 14) & 0x3FF;
                s32 lry = (w0 >>  2) & 0x3FF;
                s32 ulx = (w1 >> 14) & 0x3FF;
                s32 uly = (w1 >>  2) & 0x3FF;

                // Skip zbuffer fills - they're used to clear the N64 depth buffer
                // which we handle via glClear(GL_DEPTH_BUFFER_BIT) in gfx_begin_frame.
                // Heuristic: In FILL mode, zbuffer fill values decoded as RGBA5551 always
                // have alpha=0 (the LSB of each 16-bit z value is 0). Real color fills
                // have alpha=1.
                if (sRDP.cycleType == 3 && sRDP.fillA < 0.5f) {
                    break;  // Skip zbuffer fills
                }

                flush_triangles();

                if (sRDP.cycleType == 3) {
                    // G_CYC_FILL: use fill color
                    draw_fill_rect(ulx, uly, lrx, lry,
                                   sRDP.fillR, sRDP.fillG, sRDP.fillB, sRDP.fillA);
                } else {
                    // G_CYC_1CYCLE / G_CYC_2CYCLE: use combiner output
                    // For now, approximate with prim color
                    draw_fill_rect(ulx, uly, lrx, lry,
                                   sRDP.primR, sRDP.primG, sRDP.primB, sRDP.primA);
                }
                break;
            }

            case (u8)G_SETOTHERMODE_H: {
                // F3DEX2 encoding: w0[15:8] = 32-sft-len, w0[7:0] = len-1
                u32 sft2 = (w0 >> 8) & 0xFF;
                u32 len2 = (w0 >> 0) & 0xFF;
                u32 len  = len2 + 1;
                u32 shift = 32 - sft2 - len;
                u32 mask  = (((u32)1 << len) - 1) << shift;
                sRDP.otherModeH = (sRDP.otherModeH & ~mask) | (w1 & mask);
                // Extract cycle type (bits 20-21)
                sRDP.cycleType = (sRDP.otherModeH >> G_MDSFT_CYCLETYPE) & 0x3;
                // Extract TLUT mode (bits 14-15)
                sRDP.tlutMode = (sRDP.otherModeH >> G_MDSFT_TEXTLUT) & 0x3;
                break;
            }

            case (u8)G_SETOTHERMODE_L: {
                // F3DEX2 encoding: w0[15:8] = 32-sft-len, w0[7:0] = len-1
                u32 sft2 = (w0 >> 8) & 0xFF;
                u32 len2 = (w0 >> 0) & 0xFF;
                u32 len  = len2 + 1;
                u32 shift = 32 - sft2 - len;
                u32 mask  = (((u32)1 << len) - 1) << shift;
                sRDP.otherModeL = (sRDP.otherModeL & ~mask) | (w1 & mask);
                break;
            }

            case (u8)G_RDPSETOTHERMODE:
                // Full othermode set: w0[23:0] = otherModeH, w1 = otherModeL
                sRDP.otherModeH = w0 & 0x00FFFFFF;
                sRDP.otherModeL = w1;
                sRDP.cycleType = (sRDP.otherModeH >> G_MDSFT_CYCLETYPE) & 0x3;
                break;

            case (u8)G_SETCOMBINE:
                sRDP.combineHi = w0 & 0x00FFFFFF;
                sRDP.combineLo = w1;
                break;

            case G_TEXRECT:
            case G_TEXRECTFLIP: {
                // Texture rectangle: 3 Gfx words total
                // Word 0: [G_TEXRECT(8) | xh(12) | yh(12)] / [tile(3) | xl(12) | yl(12)]
                // Word 1: [G_RDPHALF_1(8) | 0] / [s(16) | t(16)]
                // Word 2: [G_RDPHALF_2(8) | 0] / [dsdx(16) | dtdy(16)]
                s32 xh = (w0 >> 12) & 0xFFF;   // lower-right x (10.2 fixed)
                s32 yh = w0 & 0xFFF;            // lower-right y (10.2 fixed)
                u32 rectTile = (w1 >> 24) & 0x7;
                s32 xl = (w1 >> 12) & 0xFFF;    // upper-left x (10.2 fixed)
                s32 yl = w1 & 0xFFF;            // upper-left y (10.2 fixed)

                // Read s/t from next word (G_RDPHALF_1)
                i++;
                u32 w0_st = (u32)dl[i].words.w0;
                u32 w1_st = (u32)dl[i].words.w1;
                s16 texS = (s16)((w1_st >> 16) & 0xFFFF);
                s16 texT = (s16)(w1_st & 0xFFFF);

                // Read dsdx/dtdy from next word (G_RDPHALF_2)
                i++;
                u32 w1_ds = (u32)dl[i].words.w1;
                s16 dsdx = (s16)((w1_ds >> 16) & 0xFFFF);
                s16 dtdy = (s16)(w1_ds & 0xFFFF);

                // Convert screen coords from 10.2 fixed point to pixels
                f32 rx0 = xl / 4.0f;   // upper-left x
                f32 ry0 = yl / 4.0f;   // upper-left y
                f32 rx1 = xh / 4.0f;   // lower-right x
                f32 ry1 = yh / 4.0f;   // lower-right y
                f32 rectW = rx1 - rx0;
                f32 rectH = ry1 - ry0;
                if (rectW <= 0 || rectH <= 0) break;

                // Texture coords: s/t are s10.5, dsdx/dtdy are s5.10
                f32 s0 = texS / 32.0f;
                f32 t0 = texT / 32.0f;
                f32 ds = dsdx / 1024.0f;
                f32 dt = dtdy / 1024.0f;

                // In COPY mode, dsdx is 4x normal (hardware quirk)
                if (sRDP.cycleType == 2) {
                    ds /= 4.0f;
                }

                // Calculate texture coords at corners
                f32 sUL, tUL, sUR, tUR, sLL, tLL, sLR, tLR;
                if (opcode == G_TEXRECTFLIP) {
                    // FLIP: S advances down, T advances right
                    sUL = s0;             tUL = t0;
                    sUR = s0;             tUR = t0 + dt * rectW;
                    sLL = s0 + ds * rectH; tLL = t0;
                    sLR = s0 + ds * rectH; tLR = t0 + dt * rectW;
                } else {
                    // Normal: S advances right, T advances down
                    sUL = s0;             tUL = t0;
                    sUR = s0 + ds * rectW; tUR = t0;
                    sLL = s0;             tLL = t0 + dt * rectH;
                    sLR = s0 + ds * rectW; tLR = t0 + dt * rectH;
                }

                // Upload texture for this tile if needed
                if (rectTile < N64_TILE_COUNT && sRDP.tiles[rectTile].sizeSet) {
                    if (sTileTexGeneration[rectTile] != sTmemGeneration || sTileTextures[rectTile] == 0) {
                        upload_tile_texture(rectTile);
                    }
                }

                // Get tile dimensions for normalizing texture coords
                TileDescriptor* td = &sRDP.tiles[rectTile];
                f32 tileW = ((td->lrs - td->uls) / 4.0f) + 1.0f;
                f32 tileH = ((td->lrt - td->ult) / 4.0f) + 1.0f;
                if (tileW <= 0) tileW = 1.0f;
                if (tileH <= 0) tileH = 1.0f;

                // Normalize to 0-1 UV range
                f32 uvUL_s = sUL / tileW, uvUL_t = tUL / tileH;
                f32 uvUR_s = sUR / tileW, uvUR_t = tUR / tileH;
                f32 uvLL_s = sLL / tileW, uvLL_t = tLL / tileH;
                f32 uvLR_s = sLR / tileW, uvLR_t = tLR / tileH;

                // Convert N64 screen coords to OpenGL NDC
                f32 ndcX0 = rx0 / 160.0f - 1.0f;
                f32 ndcY0 = 1.0f - ry0 / 120.0f;
                f32 ndcX1 = rx1 / 160.0f - 1.0f;
                f32 ndcY1 = 1.0f - ry1 / 120.0f;

                // For vertex color, use white (combiner handles the rest)
                f32 cr = 1.0f, cg = 1.0f, cb = 1.0f, ca = 1.0f;

                // Flush current batch (switching to 2D rect mode)
                flush_triangles();

                // Set texture and combiner state
                s32 rectUseTexture = (sTileTextures[rectTile] != 0);
                sTriBatchTextured = rectUseTexture;
                sTriBatchTexId = sTileTextures[rectTile];
                sTriBatchPrimColor[0] = sRDP.primR; sTriBatchPrimColor[1] = sRDP.primG;
                sTriBatchPrimColor[2] = sRDP.primB; sTriBatchPrimColor[3] = sRDP.primA;
                sTriBatchEnvColor[0] = sRDP.envR; sTriBatchEnvColor[1] = sRDP.envG;
                sTriBatchEnvColor[2] = sRDP.envB; sTriBatchEnvColor[3] = sRDP.envA;
                if (sRDP.cycleType == 2) {
                    // G_CYC_COPY: texel color directly (a=TEX, b=0, c=0, d=0 → just TEX)
                    // But (a-b)*c+d with c=0 = d. Use a=TEX, b=0, c=ONE, d=0 instead.
                    sTriBatchCombColor[0] = COMB_TEXEL0; sTriBatchCombColor[1] = COMB_ZERO;
                    sTriBatchCombColor[2] = COMB_ONE;    sTriBatchCombColor[3] = COMB_ZERO;
                    sTriBatchCombAlpha[0] = COMB_TEXEL0; sTriBatchCombAlpha[1] = COMB_ZERO;
                    sTriBatchCombAlpha[2] = COMB_ONE;    sTriBatchCombAlpha[3] = COMB_ZERO;
                } else {
                    decode_combine_mode(sRDP.combineHi, sRDP.combineLo,
                                        sTriBatchCombColor, sTriBatchCombAlpha);
                }

                // Force depth off for 2D textured rects
                sTriBatchOtherModeL = sRDP.otherModeL & ~0x0030;  // Clear Z_CMP and Z_UPD

                // Emit two triangles as a quad
                f32* dst = &sTriBatch[sTriBatchCount * 3 * TRI_VERTEX_FLOATS];

                // Triangle 1: UL, UR, LR
                // UL
                *dst++ = ndcX0; *dst++ = ndcY0; *dst++ = 0.0f; *dst++ = 1.0f;
                *dst++ = cr; *dst++ = cg; *dst++ = cb; *dst++ = ca;
                *dst++ = uvUL_s; *dst++ = uvUL_t; *dst++ = 0.0f;
                // UR
                *dst++ = ndcX1; *dst++ = ndcY0; *dst++ = 0.0f; *dst++ = 1.0f;
                *dst++ = cr; *dst++ = cg; *dst++ = cb; *dst++ = ca;
                *dst++ = uvUR_s; *dst++ = uvUR_t; *dst++ = 0.0f;
                // LR
                *dst++ = ndcX1; *dst++ = ndcY1; *dst++ = 0.0f; *dst++ = 1.0f;
                *dst++ = cr; *dst++ = cg; *dst++ = cb; *dst++ = ca;
                *dst++ = uvLR_s; *dst++ = uvLR_t; *dst++ = 0.0f;

                // Triangle 2: UL, LR, LL
                // UL
                *dst++ = ndcX0; *dst++ = ndcY0; *dst++ = 0.0f; *dst++ = 1.0f;
                *dst++ = cr; *dst++ = cg; *dst++ = cb; *dst++ = ca;
                *dst++ = uvUL_s; *dst++ = uvUL_t; *dst++ = 0.0f;
                // LR
                *dst++ = ndcX1; *dst++ = ndcY1; *dst++ = 0.0f; *dst++ = 1.0f;
                *dst++ = cr; *dst++ = cg; *dst++ = cb; *dst++ = ca;
                *dst++ = uvLR_s; *dst++ = uvLR_t; *dst++ = 0.0f;
                // LL
                *dst++ = ndcX0; *dst++ = ndcY1; *dst++ = 0.0f; *dst++ = 1.0f;
                *dst++ = cr; *dst++ = cg; *dst++ = cb; *dst++ = ca;
                *dst++ = uvLL_s; *dst++ = uvLL_t; *dst++ = 0.0f;

                sTriBatchCount += 2;

                // Flush immediately for 2D overlay
                flush_triangles();
                break;
            }

            case G_SETTIMG: {
                // w0: [cmd(8) | fmt(3) | siz(2) | pad | width-1(12)]
                // w1: address
                sRDP.texImgFmt = (w0 >> 21) & 0x7;
                sRDP.texImgSiz = (w0 >> 19) & 0x3;
                sRDP.texImgWidth = (w0 & 0xFFF) + 1;
                sRDP.texImgAddr = (uintptr_t)dl[i].words.w1;
                break;
            }

            case G_SETTILE: {
                // w0: [cmd(8) | fmt(3) | siz(2) | pad(2) | line(9) | tmem(9)]
                // w1: [pad(5) | tile(3) | palette(4) | cmt(2) | maskt(4) | shiftt(4) | cms(2) | masks(4) | shifts(4)]
                u32 tile = (w1 >> 24) & 0x7;
                sRDP.tiles[tile].fmt = (w0 >> 21) & 0x7;
                sRDP.tiles[tile].siz = (w0 >> 19) & 0x3;
                sRDP.tiles[tile].line = (w0 >> 9) & 0x1FF;
                sRDP.tiles[tile].tmem = w0 & 0x1FF;
                sRDP.tiles[tile].palette = (w1 >> 20) & 0xF;
                sRDP.tiles[tile].cmt = (w1 >> 18) & 0x3;
                sRDP.tiles[tile].maskt = (w1 >> 14) & 0xF;
                sRDP.tiles[tile].shiftt = (w1 >> 10) & 0xF;
                sRDP.tiles[tile].cms = (w1 >> 8) & 0x3;
                sRDP.tiles[tile].masks = (w1 >> 4) & 0xF;
                sRDP.tiles[tile].shifts = w1 & 0xF;
                break;
            }

            case G_SETTILESIZE: {
                // w0: [cmd(8) | uls(12) | ult(12)]
                // w1: [pad(5) | tile(3) | lrs(12) | lrt(12)]
                u32 tile = (w1 >> 24) & 0x7;
                sRDP.tiles[tile].uls = (f32)((w0 >> 12) & 0xFFF);
                sRDP.tiles[tile].ult = (f32)(w0 & 0xFFF);
                sRDP.tiles[tile].lrs = (f32)((w1 >> 12) & 0xFFF);
                sRDP.tiles[tile].lrt = (f32)(w1 & 0xFFF);
                sRDP.tiles[tile].sizeSet = TRUE;
                break;
            }

            case G_LOADBLOCK: {
                // w0: [cmd(8) | uls(12) | ult(12)]
                // w1: [pad(5) | tile(3) | lrs(12) | dxt(12)]
                u32 tile = (w1 >> 24) & 0x7;
                u32 lrs = (w1 >> 12) & 0xFFF;

                u32 numTexels = lrs + 1;
                u32 byteCount = 0;
                switch (sRDP.texImgSiz) {
                    case G_IM_SIZ_4b:  byteCount = (numTexels + 1) / 2; break;
                    case G_IM_SIZ_8b:  byteCount = numTexels; break;
                    case G_IM_SIZ_16b: byteCount = numTexels * 2; break;
                    case G_IM_SIZ_32b: byteCount = numTexels * 4; break;
                }

                u32 tmemByteOfs = sRDP.tiles[tile].tmem * 8;
                // Safety: validate address looks like a valid PC pointer (not N64 RDRAM addr)
                // N64 RDRAM addresses are < 0x01000000; PC pointers are much larger
                if (sRDP.texImgAddr > 0x10000000ULL && byteCount > 0 &&
                    tmemByteOfs + byteCount <= TMEM_SIZE) {
                    memcpy(&sRDP.tmem[tmemByteOfs], (void*)sRDP.texImgAddr, byteCount);
                    sTmemGeneration++;
                }
                break;
            }

            case G_LOADTILE: {
                // w0: [cmd(8) | uls(12) | ult(12)]
                // w1: [pad(5) | tile(3) | lrs(12) | lrt(12)]
                u32 tile = (w1 >> 24) & 0x7;
                u32 uls = (w0 >> 12) & 0xFFF;
                u32 ult = w0 & 0xFFF;
                u32 lrs = (w1 >> 12) & 0xFFF;
                u32 lrt = w1 & 0xFFF;

                // Coords are in quarter-texel units
                s32 tileW = (lrs - uls) / 4 + 1;
                s32 tileH = (lrt - ult) / 4 + 1;
                if (tileW <= 0 || tileH <= 0) break;

                u32 bytesPerRow = 0;
                switch (sRDP.texImgSiz) {
                    case G_IM_SIZ_4b:  bytesPerRow = (tileW + 1) / 2; break;
                    case G_IM_SIZ_8b:  bytesPerRow = tileW; break;
                    case G_IM_SIZ_16b: bytesPerRow = tileW * 2; break;
                    case G_IM_SIZ_32b: bytesPerRow = tileW * 4; break;
                }

                u32 srcBytesPerRow = 0;
                switch (sRDP.texImgSiz) {
                    case G_IM_SIZ_4b:  srcBytesPerRow = (sRDP.texImgWidth + 1) / 2; break;
                    case G_IM_SIZ_8b:  srcBytesPerRow = sRDP.texImgWidth; break;
                    case G_IM_SIZ_16b: srcBytesPerRow = sRDP.texImgWidth * 2; break;
                    case G_IM_SIZ_32b: srcBytesPerRow = sRDP.texImgWidth * 4; break;
                }

                u32 tmemByteOfs = sRDP.tiles[tile].tmem * 8;
                u32 totalBytes = bytesPerRow * tileH;

                // Safety: validate address looks like a valid PC pointer
                if (sRDP.texImgAddr > 0x10000000ULL && totalBytes > 0 &&
                    tmemByteOfs + totalBytes <= TMEM_SIZE) {
                    const u8* srcBase = (const u8*)sRDP.texImgAddr;
                    u32 srcStartRow = ult / 4;
                    u32 srcStartCol = uls / 4;
                    u32 srcColByteOfs = 0;
                    switch (sRDP.texImgSiz) {
                        case G_IM_SIZ_4b:  srcColByteOfs = srcStartCol / 2; break;
                        case G_IM_SIZ_8b:  srcColByteOfs = srcStartCol; break;
                        case G_IM_SIZ_16b: srcColByteOfs = srcStartCol * 2; break;
                        case G_IM_SIZ_32b: srcColByteOfs = srcStartCol * 4; break;
                    }
                    for (s32 row = 0; row < tileH; row++) {
                        u32 srcOfs = (srcStartRow + row) * srcBytesPerRow + srcColByteOfs;
                        u32 dstOfs = tmemByteOfs + row * bytesPerRow;
                        memcpy(&sRDP.tmem[dstOfs], srcBase + srcOfs, bytesPerRow);
                    }
                    sTmemGeneration++;
                }
                break;
            }

            case G_LOADTLUT: {
                // w0: [cmd(8) | pad(24)]
                // w1: [pad(5) | tile(3) | count(10) | pad(14)]
                u32 tile = (w1 >> 24) & 0x7;
                u32 count = ((w1 >> 14) & 0x3FF) + 1; // Number of palette entries

                // TLUT data was set via G_SETTIMG prior to this command
                // The tile's tmem field tells us where in TMEM to store the palette
                u32 tmemByteOfs = sRDP.tiles[tile].tmem * 8;
                u32 byteCount = count * 2; // Each palette entry is 16-bit RGBA5551

                // Safety: validate address looks like a valid PC pointer
                if (sRDP.texImgAddr > 0x10000000ULL && byteCount > 0 &&
                    tmemByteOfs + byteCount <= TMEM_SIZE) {
                    memcpy(&sRDP.tmem[tmemByteOfs], (void*)sRDP.texImgAddr, byteCount);
                    sTmemGeneration++;
                }
                break;
            }

            case G_SETZIMG:
                sRDP.zimgAddr = (uintptr_t)dl[i].words.w1;
                break;

            case G_SETCIMG:
                sRDP.cimgAddr = (uintptr_t)dl[i].words.w1;
                sRDP.renderingToZbuf = (sRDP.cimgAddr == sRDP.zimgAddr && sRDP.zimgAddr != 0);
                break;

            case G_TEXTURE: {
                // F3DEX2: w0 = [cmd(8) | bowtie(8) | level(3) | tile(3) | on(7) | pad(3)]
                // w1 = [scaleS(16) | scaleT(16)]
                flush_triangles();
                sRSP.texLevel = (w0 >> 11) & 0x7;
                sRSP.texTile = (w0 >> 8) & 0x7;
                sRSP.texEnabled = ((w0 >> 1) & 0x7F) != 0;
                sRSP.texScaleS = (w1 >> 16) & 0xFFFF;
                sRSP.texScaleT = w1 & 0xFFFF;
                break;
            }

            case G_MOVEWORD: {
                // F3DEX2: w0 = [DB | offset(16) | idx(8)]
                // w1 = value
                u8 mwIdx = w0 & 0xFF;
                u16 mwOfs = (w0 >> 8) & 0xFFFF;
                if (mwIdx == G_MW_FOG && mwOfs == G_MWO_FOG) {
                    // Fog parameters: w1 = [fogMul(16) | fogOfs(16)]
                    sRSP.fogMul = (s16)((w1 >> 16) & 0xFFFF);
                    sRSP.fogOfs = (s16)(w1 & 0xFFFF);
                }
                break;
            }

            case G_SETBLENDCOLOR:
                // w1: [R(8) | G(8) | B(8) | A(8)]
                sRDP.blendR = ((w1 >> 24) & 0xFF) / 255.0f;
                sRDP.blendG = ((w1 >> 16) & 0xFF) / 255.0f;
                sRDP.blendB = ((w1 >>  8) & 0xFF) / 255.0f;
                sRDP.blendA = ((w1 >>  0) & 0xFF) / 255.0f;
                break;

            case G_SETSCISSOR:
            case G_SETPRIMDEPTH:
            case G_NOOP:
                // Misc commands - skip for now
                break;

            default:
                // Unknown command - skip silently
                break;
        }
    }
    flush_triangles();
}

// ============================================================================
// Public API
// ============================================================================

void rdp_init(void) {
    memset(&sRDP, 0, sizeof(sRDP));
    memset(&sRSP, 0, sizeof(sRSP));

    // Initialize matrix stacks to identity
    mtx_identity(sRSP.modelview[0]);
    mtx_identity(sRSP.projection[0]);
    mtx_identity(sRSP.mvp);
    sRSP.modelviewTop = 0;
    sRSP.projectionTop = 0;
    sRSP.mvpDirty = TRUE;

    // Default viewport (320x240)
    sRSP.vpScaleX = 160.0f;
    sRSP.vpScaleY = 120.0f;
    sRSP.vpScaleZ = 511.0f;
    sRSP.vpTransX = 160.0f;
    sRSP.vpTransY = 120.0f;
    sRSP.vpTransZ = 511.0f;

    // Default texture scale
    sRSP.texScaleS = 0xFFFF;
    sRSP.texScaleT = 0xFFFF;

    // Initialize tile texture array
    memset(sTileTextures, 0, sizeof(sTileTextures));
    memset(sTileTexWidth, 0, sizeof(sTileTexWidth));
    memset(sTileTexHeight, 0, sizeof(sTileTexHeight));
    memset(sTileTexGeneration, 0, sizeof(sTileTexGeneration));
    sTmemGeneration = 0;

    init_shaders();
}

void rdp_process_display_list(Gfx* dl, u32 sizeBytes) {
    if (!dl) return;

    sFrameNum++;

    // Initialize shaders on first call
    if (!sShadersReady) {
        init_shaders();
        if (!sShadersReady) {
            if (sFrameNum <= 5) fprintf(stderr, "[RDP] Shaders not ready, skipping DL\n");
            return;
        }
    }

    if (sFrameNum == 1) {
        fprintf(stderr, "[RDP] First display list: %lu bytes, %lu commands\n",
                (unsigned long)sizeBytes, (unsigned long)(sizeBytes/8));
    }

    // Reset RDP state for this display list
    sRDP.cycleType = 0;
    sRDP.fillColor = 0;
    sRDP.fillR = sRDP.fillG = sRDP.fillB = 0.0f;
    sRDP.fillA = 1.0f;
    sRDP.primR = sRDP.primG = sRDP.primB = 0.0f;
    sRDP.primA = 1.0f;
    sRDP.envR = sRDP.envG = sRDP.envB = 0.0f;
    sRDP.envA = 1.0f;
    sRDP.cimgAddr = 0;
    sRDP.zimgAddr = 0;
    sRDP.renderingToZbuf = FALSE;
    sRDP.texImgAddr = 0;
    sRDP.texImgFmt = 0;
    sRDP.texImgSiz = 0;
    sRDP.texImgWidth = 0;
    sRDP.tlutMode = 0;
    memset(sRDP.tiles, 0, sizeof(sRDP.tiles));
    // Don't clear TMEM - textures may persist across display lists

    // Reset RSP state
    sRSP.modelviewTop = 0;
    sRSP.projectionTop = 0;
    mtx_identity(sRSP.modelview[0]);
    mtx_identity(sRSP.projection[0]);
    sRSP.mvpDirty = TRUE;
    sRSP.geometryMode = 0;
    sRSP.texEnabled = FALSE;
    sRSP.texScaleS = 0xFFFF;
    sRSP.texScaleT = 0xFFFF;
    sRSP.texTile = 0;
    sRSP.texLevel = 0;
    sTriBatchCount = 0;
    sTriBatchTextured = FALSE;
    sTriBatchTexId = 0;
    // Default combiner: SHADE only (a=SHADE, b=0, c=ONE, d=0)
    sTriBatchCombColor[0] = COMB_SHADE; sTriBatchCombColor[1] = COMB_ZERO;
    sTriBatchCombColor[2] = COMB_ONE;   sTriBatchCombColor[3] = COMB_ZERO;
    sTriBatchCombAlpha[0] = COMB_SHADE; sTriBatchCombAlpha[1] = COMB_ZERO;
    sTriBatchCombAlpha[2] = COMB_ONE;   sTriBatchCombAlpha[3] = COMB_ZERO;
    sTriBatchPrimColor[0] = 0; sTriBatchPrimColor[1] = 0;
    sTriBatchPrimColor[2] = 0; sTriBatchPrimColor[3] = 1;
    sTriBatchEnvColor[0] = 0; sTriBatchEnvColor[1] = 0;
    sTriBatchEnvColor[2] = 0; sTriBatchEnvColor[3] = 1;
    sTriBatchOtherModeL = 0;

    // Reset per-frame triangle stats
    sFrameTotalTris = 0;
    sFrameOnScreenTris = 0;

    // Calculate number of commands
    // Note: sizeBytes uses the N64 Gfx size (8 bytes per command) since the game code
    // hardcodes "* 8" in the nuGfxTaskStart call. On PC, sizeof(Gfx) is 16 bytes.
    u32 numCommands = sizeBytes / 8;
    if (numCommands == 0) return;

    rdp_process_dl(dl, numCommands);

    // Log first frame with triangles and periodic stats
    {
        static s32 sFirstTriReported = FALSE;
        static s32 sFirstOnScreenReported = FALSE;
        if (sFrameTotalTris > 0 && !sFirstTriReported) {
            fprintf(stderr, "[RDP] First geometry at DL %lu: %ld tris (%ld on-screen)\n",
                    (unsigned long)sFrameNum, (long)sFrameTotalTris, (long)sFrameOnScreenTris);
            sFirstTriReported = TRUE;
        }
        if (sFrameOnScreenTris > 0 && !sFirstOnScreenReported) {
            fprintf(stderr, "[RDP] First on-screen tris at DL %lu: %ld total, %ld visible, texEnabled=%d\n",
                    (unsigned long)sFrameNum, (long)sFrameTotalTris, (long)sFrameOnScreenTris,
                    sRSP.texEnabled);
            sFirstOnScreenReported = TRUE;
        }
    }

}
