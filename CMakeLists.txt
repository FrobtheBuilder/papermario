cmake_minimum_required(VERSION 3.16)
project(papermario C)

# Use GNU11 (not strict C11) to allow GNU extensions like
# address-to-integer casts in static initializers, which the
# EVT script system relies on heavily
set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_C_EXTENSIONS ON)

# Platform detection
if(WIN32)
    set(PLATFORM_WINDOWS TRUE)
    add_definitions(-DPLATFORM_PC -DPLATFORM_WINDOWS -D_CRT_SECURE_NO_WARNINGS)
else()
    add_definitions(-DPLATFORM_PC)
endif()

# N64 SDK compatibility defines (required for original decomp headers)
add_definitions(
    -D_LANGUAGE_C
    -DF3DEX_GBI_2
    -DVERSION_US
    -DVERSION=us
    -D_FINALROM
)


# Find SDL2
find_package(SDL2 REQUIRED)
if(NOT SDL2_FOUND)
    message(FATAL_ERROR "SDL2 not found. Install with: pacman -S mingw-w64-ucrt-x86_64-SDL2")
endif()

# Find OpenGL
find_package(OpenGL REQUIRED)

# Find GLEW
find_package(GLEW REQUIRED)
if(NOT GLEW_FOUND)
    message(FATAL_ERROR "GLEW not found. Install with: pacman -S mingw-w64-ucrt-x86_64-glew")
endif()

# Find SDL2_mixer
find_library(SDL2_MIXER_LIBRARY SDL2_mixer REQUIRED)
if(NOT SDL2_MIXER_LIBRARY)
    message(FATAL_ERROR "SDL2_mixer not found. Install with: pacman -S mingw-w64-ucrt-x86_64-SDL2_mixer")
endif()

# PC platform sources
set(PC_SOURCES
    src/pc/platform/pc_main.c
    src/pc/platform/gu_math_pc.c
    src/pc/platform/n64_stubs.c
    src/pc/platform/overlay_stubs.c
    src/pc/platform/effect_stubs.c
    src/pc/platform/shim_stubs.c
    src/pc/platform/asset_stubs.c
    src/pc/gfx/pc_gfx.c
    src/pc/gfx/rdp_translator.c
    src/pc/input/pc_input.c
    src/pc/audio/pc_audio.c
)

# N64 SDK math functions (portable C implementations from decomp)
set(N64_MATH_SOURCES
    src/os/guRotate.c
    src/os/guMtxCat.c
    src/os/guMtxXFMF.c
    src/os/guMtxXFML.c
    src/os/perspective.c
    src/os/frustum.c
    src/os/guOrtho.c
    src/os/guLookAt.c
    src/os/lookathil.c
    src/os/lookatref.c
    src/os/position.c
    src/os/rotateRPY.c
)

# Game sources - all subdirectories except os/ (handled separately) and pc/ (handled above)
file(GLOB GAME_SRC_ROOT src/*.c)
file(GLOB_RECURSE GAME_SRC_AUDIO src/audio/*.c)
file(GLOB_RECURSE GAME_SRC_BATTLE src/battle/*.c)
# BSS files excluded - they are N64 linker ordering artifacts that cause multiple definition errors
file(GLOB_RECURSE GAME_SRC_CHARSET src/charset/*.c)
file(GLOB_RECURSE GAME_SRC_COMMON src/common/*.c)
file(GLOB_RECURSE GAME_SRC_EFFECTS src/effects/*.c)
file(GLOB_RECURSE GAME_SRC_ENTITY src/entity/*.c)
file(GLOB_RECURSE GAME_SRC_EVT src/evt/*.c)
file(GLOB_RECURSE GAME_SRC_FILEMENU src/filemenu/*.c)
file(GLOB_RECURSE GAME_SRC_PAUSE src/pause/*.c)
file(GLOB_RECURSE GAME_SRC_WORLD src/world/*.c)
set(GAME_SOURCES
    ${GAME_SRC_ROOT}
    ${GAME_SRC_AUDIO}
    ${GAME_SRC_BATTLE}
    ${GAME_SRC_CHARSET}
    ${GAME_SRC_COMMON}
    ${GAME_SRC_EFFECTS}
    ${GAME_SRC_ENTITY}
    ${GAME_SRC_EVT}
    ${GAME_SRC_FILEMENU}
    ${GAME_SRC_PAUSE}
    ${GAME_SRC_WORLD}
)

# Exclude .inc.c files (they are #include'd into other .c files, not standalone)
list(FILTER GAME_SOURCES EXCLUDE REGEX "\\.inc\\.c$")

# Exclude PAL-only files (PAL assets not available for US version)
list(FILTER GAME_SOURCES EXCLUDE REGEX "level_up_letters_(es|fr|en_de)\\.c$")
list(FILTER GAME_SOURCES EXCLUDE REGEX "starpoint_(de|es|fr|en)\\.c$")
list(FILTER GAME_SOURCES EXCLUDE REGEX "attack_result_text_(de|es|fr)\\.c$")
list(FILTER GAME_SOURCES EXCLUDE REGEX "chapter_change_(de|es|fr)\\.c$")
list(FILTER GAME_SOURCES EXCLUDE REGEX "filemenu_selectlanguage\\.c$")

# Exclude JP-only files (JP messages not available for US version)
list(FILTER GAME_SOURCES EXCLUDE REGEX "hos_10/narrator\\.c$")

# Exclude dead/unused older versions of areas (references undefined symbols, not needed for port)
list(FILTER GAME_SOURCES EXCLUDE REGEX "/dead/")

# Exclude .c files that are #include'd by other .c files (pseudo-.inc.c files)
# These are compiled as part of their parent translation unit, not standalone
list(FILTER GAME_SOURCES EXCLUDE REGEX "effects/effect_table\\.c$")
list(FILTER GAME_SOURCES EXCLUDE REGEX "dro_01/npc_hint_dryite\\.c$")
list(FILTER GAME_SOURCES EXCLUDE REGEX "dro_01/npc_hint_dryite_companion\\.c$")
list(FILTER GAME_SOURCES EXCLUDE REGEX "dro_01/npc_composer\\.c$")
list(FILTER GAME_SOURCES EXCLUDE REGEX "dro_01/npc_shop_owner\\.c$")
list(FILTER GAME_SOURCES EXCLUDE REGEX "dro_02/npc_merlee\\.c$")
list(FILTER GAME_SOURCES EXCLUDE REGEX "dro_02/npc_moustafa\\.c$")
list(FILTER GAME_SOURCES EXCLUDE REGEX "kzn_19/kzn_19_anim[123]\\.c$")

# Exclude BSS ordering files (N64 linker artifacts, variables already defined in proper source files)
list(FILTER GAME_SOURCES EXCLUDE REGEX "battle_bss\\.c$")

# Include directories
set(INCLUDE_DIRS
    ${CMAKE_SOURCE_DIR}/include
    ${CMAKE_SOURCE_DIR}/src
    ${CMAKE_SOURCE_DIR}/ver/us/build/include
    ${CMAKE_SOURCE_DIR}/assets/us
    ${SDL2_INCLUDE_DIRS}
    ${OPENGL_INCLUDE_DIR}
    ${GLEW_INCLUDE_DIRS}
)

# Generated asset data (PNG -> N64 raw format)
file(GLOB GENERATED_ASSET_SOURCES ${CMAKE_SOURCE_DIR}/src/pc/gen/asset_data_*.c)

# Create executable
add_executable(papermario ${PC_SOURCES} ${N64_MATH_SOURCES} ${GAME_SOURCES} ${GENERATED_ASSET_SOURCES})

target_include_directories(papermario PRIVATE ${INCLUDE_DIRS})

# Link libraries
target_link_libraries(papermario
    ${SDL2_LIBRARIES}
    ${OPENGL_LIBRARIES}
    ${GLEW_LIBRARIES}
    ${SDL2_MIXER_LIBRARY}
)

# Platform-specific settings
if(PLATFORM_WINDOWS)
    target_link_libraries(papermario winmm)

    # Console subsystem for printf output
    # -pipe avoids temp file creation issues in sandboxed environments
    set_target_properties(papermario PROPERTIES
        LINK_FLAGS "-mconsole -pipe"
    )
endif()

# Compiler flags
target_compile_options(papermario PRIVATE
    -pipe
    -Wall
    -Wextra
    -Wno-unused-parameter
    -Wno-unused-variable
    -Wno-missing-field-initializers
    -Wno-sign-compare
    -Wno-missing-braces
    -Wno-implicit-function-declaration
    -Wno-incompatible-pointer-types
    -Wno-pointer-to-int-cast
    -Wno-int-to-pointer-cast
    -Wno-builtin-declaration-mismatch
    -Wno-return-mismatch
    -Wno-int-conversion
    -Wno-error
    -O2
)

# Copy required DLLs to build directory (Windows only)
if(PLATFORM_WINDOWS)
    # Find the UCRT64 bin directory from the compiler path
    get_filename_component(COMPILER_DIR ${CMAKE_C_COMPILER} DIRECTORY)

    set(REQUIRED_DLLS
        SDL2.dll
        SDL2_mixer.dll
        glew32.dll
        libFLAC.dll
        libmpg123-0.dll
        libogg-0.dll
        libopus-0.dll
        libopusfile-0.dll
        libvorbis-0.dll
        libvorbisfile-3.dll
        libwavpack-1.dll
        libwinpthread-1.dll
        libxmp.dll
    )

    foreach(DLL ${REQUIRED_DLLS})
        if(EXISTS "${COMPILER_DIR}/${DLL}")
            add_custom_command(TARGET papermario POST_BUILD
                COMMAND ${CMAKE_COMMAND} -E copy_if_different
                "${COMPILER_DIR}/${DLL}" $<TARGET_FILE_DIR:papermario>
            )
        endif()
    endforeach()
endif()

# Print configuration
message(STATUS "=== Paper Mario PC Port Configuration ===")
message(STATUS "SDL2: ${SDL2_INCLUDE_DIRS}")
message(STATUS "GLEW: ${GLEW_INCLUDE_DIRS}")
message(STATUS "SDL2_mixer: ${SDL2_MIXER_LIBRARY}")
message(STATUS "Build type: ${CMAKE_BUILD_TYPE}")
message(STATUS "C Compiler: ${CMAKE_C_COMPILER}")
message(STATUS "=========================================")
