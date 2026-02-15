/**
 * gu_math_pc.c - Portable C implementations of libultra math functions
 *
 * These functions are originally in MIPS assembly (.s files) in the N64 SDK.
 * This file provides equivalent C implementations for the PC port.
 */

#include "PR/guint.h"
#include <math.h>

void guMtxIdentF(float mf[4][4]) {
    int i, j;

    for (i = 0; i < 4; i++) {
        for (j = 0; j < 4; j++) {
            mf[i][j] = (i == j) ? 1.0f : 0.0f;
        }
    }
}

void guMtxIdent(Mtx *m) {
    float mf[4][4];

    guMtxIdentF(mf);
    guMtxF2L(mf, m);
}

/**
 * Convert a 4x4 float matrix to the N64 fixed-point Mtx format.
 *
 * N64 Mtx is s15.16 fixed-point stored in an interleaved layout:
 *   m[0][0..3] and m[1][0..3] = integer halves (rows 0-3, two elements packed per word)
 *   m[2][0..3] and m[3][0..3] = fractional halves (rows 0-3, two elements packed per word)
 */
void guMtxF2L(float mf[4][4], Mtx *m) {
    int i, j;
    int e1, e2;
    int *ai, *af;

    ai = (int *)&m->m[0][0];
    af = (int *)&m->m[2][0];

    for (i = 0; i < 4; i++) {
        for (j = 0; j < 2; j++) {
            e1 = ROUND(mf[i][j * 2] * 65536.0f);
            e2 = ROUND(mf[i][j * 2 + 1] * 65536.0f);
            *(ai++) = (e1 & 0xffff0000) | ((e2 >> 16) & 0xffff);
            *(af++) = ((e1 << 16) & 0xffff0000) | (e2 & 0xffff);
        }
    }
}

/**
 * Convert an N64 fixed-point Mtx to a 4x4 float matrix.
 */
void guMtxL2F(float mf[4][4], Mtx *m) {
    int i, j;
    unsigned int e1, e2;
    unsigned int *ai, *af;

    ai = (unsigned int *)&m->m[0][0];
    af = (unsigned int *)&m->m[2][0];

    for (i = 0; i < 4; i++) {
        for (j = 0; j < 2; j++) {
            e1 = (*ai & 0xffff0000) | ((*af >> 16) & 0xffff);
            e2 = ((*ai++ << 16) & 0xffff0000) | (*af++ & 0xffff);
            mf[i][j * 2] = (int)e1 / 65536.0f;
            mf[i][j * 2 + 1] = (int)e2 / 65536.0f;
        }
    }
}

/**
 * 4x4 float matrix concatenation (multiplication): r = m * n
 * Uses a temp buffer so r can alias m or n.
 */
void guMtxCatF(float m[4][4], float n[4][4], float r[4][4]) {
    float temp[4][4];
    int i, j, k;

    for (i = 0; i < 4; i++) {
        for (j = 0; j < 4; j++) {
            temp[i][j] = 0.0f;
            for (k = 0; k < 4; k++) {
                temp[i][j] += m[i][k] * n[k][j];
            }
        }
    }

    for (i = 0; i < 4; i++) {
        for (j = 0; j < 4; j++) {
            r[i][j] = temp[i][j];
        }
    }
}

void guTranslateF(float mf[4][4], float x, float y, float z) {
    guMtxIdentF(mf);
    mf[3][0] = x;
    mf[3][1] = y;
    mf[3][2] = z;
}

void guTranslate(Mtx *m, float x, float y, float z) {
    float mf[4][4];

    guTranslateF(mf, x, y, z);
    guMtxF2L(mf, m);
}

void guScaleF(float mf[4][4], float x, float y, float z) {
    guMtxIdentF(mf);
    mf[0][0] = x;
    mf[1][1] = y;
    mf[2][2] = z;
}

void guScale(Mtx *m, float x, float y, float z) {
    float mf[4][4];

    guScaleF(mf, x, y, z);
    guMtxF2L(mf, m);
}

void guNormalize(float *x, float *y, float *z) {
    float len = sqrtf(*x * *x + *y * *y + *z * *z);

    if (len > 0.0f) {
        len = 1.0f / len;
        *x *= len;
        *y *= len;
        *z *= len;
    }
}
