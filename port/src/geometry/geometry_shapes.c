/*
 * geometry_shapes.c - geometrías de prueba reutilizables (portable).
 *
 * Véase gex/geometry_shapes.h. Sin Windows, sin D3D11, sin Win32.
 */

#include "gex/geometry_shapes.h"

#include <math.h>
#include <stdlib.h>
#include <string.h>

/* Centro y tamaño del espacio lógico de la emisión (el del original:
 * cx = 256, cy = 192, puerta de pantalla 512x384). */
#define SHAPE_CENTER_X 256.0f
#define SHAPE_CENTER_Y 192.0f
#define SHAPE_HALF_SIZE 90.0f

/* Profundidad media del cubo en la escala de emisión. */
#define SHAPE_Z_BASE 0x2000
#define SHAPE_Z_SCALE 0x1000

/* Inclinación fija alrededor de X: se ven tres caras a la vez. */
#define SHAPE_TILT 0.35f

/* Las seis caras del cubo, con su color base AARRGGBB. */
static const uint32_t SHAPE_FACE_COLOR[6] = {
    0xFFE04040u, /* +X: rojo    */
    0xFF40E040u, /* -X: verde   */
    0xFF4060E0u, /* +Y: azul    */
    0xFFE0E040u, /* -Y: amarillo*/
    0xFF40E0E0u, /* +Z: cian    */
    0xFFE040E0u  /* -Z: magenta */
};

/* Índices de las 8 esquinas del cubo: ±1 en cada eje. */
static const float SHAPE_CORNER[8][3] = {
    { -1.0f, -1.0f, -1.0f },
    {  1.0f, -1.0f, -1.0f },
    {  1.0f,  1.0f, -1.0f },
    { -1.0f,  1.0f, -1.0f },
    { -1.0f, -1.0f,  1.0f },
    {  1.0f, -1.0f,  1.0f },
    {  1.0f,  1.0f,  1.0f },
    { -1.0f,  1.0f,  1.0f }
};

/* Las 6 caras, con el índice de sus 4 esquinas (en sentido antihorario
 * visto desde fuera) y el índice de su color. */
static const int SHAPE_FACE[6][5] = {
    { 1, 2, 6, 5, 0 }, /* +X */
    { 4, 0, 3, 7, 1 }, /* -X */
    { 3, 2, 6, 7, 2 }, /* +Y */
    { 4, 5, 1, 0, 3 }, /* -Y */
    { 4, 7, 6, 5, 4 }, /* +Z */
    { 0, 1, 2, 3, 5 }  /* -Z */
};

/* Multiplica un color base por un factor de sombra, canal a canal. */
static uint32_t shade_color(uint32_t color, float factor)
{
    uint32_t r = (color >> 16) & 0xFFu;
    uint32_t g = (color >> 8) & 0xFFu;
    uint32_t b = color & 0xFFu;
    uint32_t a = (color >> 24) & 0xFFu;

    r = (uint32_t)((float)r * factor + 0.5f);
    g = (uint32_t)((float)g * factor + 0.5f);
    b = (uint32_t)((float)b * factor + 0.5f);
    if (r > 255u) r = 255u;
    if (g > 255u) g = 255u;
    if (b > 255u) b = 255u;
    return (a << 24) | (r << 16) | (g << 8) | b;
}

/* Rota un punto alrededor de Y y luego alrededor de X (inclinación fija). */
static void rotate_point(const float in[3], float angle, float out[3])
{
    float c = cosf(angle);
    float s = sinf(angle);
    float x = in[0] * c + in[2] * s;
    float z = -in[0] * s + in[2] * c;
    float ct = cosf(SHAPE_TILT);
    float st = sinf(SHAPE_TILT);
    float y = in[1] * ct - z * st;

    out[0] = x;
    out[1] = y;
    out[2] = in[1] * st + z * ct;
}

int gex_shape_cube(gex_emit_output *out, float angle_radians)
{
    gex_emit_triangle *tris;
    float rotated[8][3];
    float light[3];
    float light_len;
    int face_order[6];
    float face_depth[6];
    int i;
    int f;

    if (out == NULL) {
        return -1;
    }
    memset(out, 0, sizeof(*out));

    tris = (gex_emit_triangle *)calloc(GEX_SHAPE_CUBE_TRIANGLES, sizeof(*tris));
    if (tris == NULL) {
        return -1;
    }
    out->tris = tris;
    out->count = GEX_SHAPE_CUBE_TRIANGLES;
    out->capacity = GEX_SHAPE_CUBE_TRIANGLES;

    for (i = 0; i < 8; i++) {
        rotate_point(SHAPE_CORNER[i], angle_radians, rotated[i]);
    }

    /* Luz fija desde el frente-arriba-izquierda: da relieve a las caras. */
    light[0] = -0.45f;
    light[1] = 0.75f;
    light[2] = -0.60f;
    light_len = sqrtf(light[0] * light[0] + light[1] * light[1] +
                      light[2] * light[2]);
    if (light_len > 0.0f) {
        light[0] /= light_len;
        light[1] /= light_len;
        light[2] /= light_len;
    }

    /* Profundidad media de cada cara y orden del pintor: de atrás (z mayor)
     * hacia delante. */
    for (f = 0; f < 6; f++) {
        float sum = 0.0f;
        for (i = 0; i < 4; i++) {
            sum += rotated[SHAPE_FACE[f][i]][2];
        }
        face_depth[f] = sum * 0.25f;
        face_order[f] = f;
    }
    for (f = 1; f < 6; f++) {
        int key = face_order[f];
        float key_depth = face_depth[key];
        int j = f - 1;
        while (j >= 0 && face_depth[face_order[j]] < key_depth) {
            face_order[j + 1] = face_order[j];
            j--;
        }
        face_order[j + 1] = key;
    }

    {
        size_t slot = 0;
        for (f = 0; f < 6; f++) {
            int face = face_order[f];
            const int *q = SHAPE_FACE[face];
            float normal[3];
            float dot;
            float factor;
            uint32_t color;
            int k;

            /* Normal de la cara: el producto vectorial de dos aristas. Las
             * aristas miden 2 unidades, así que el producto vectorial NO es
             * unitario: hay que normalizarlo o el factor de sombreado se
             * dispara (las caras salen quemadas al saturar a 255). */
            {
                float e1[3], e2[3];
                float len;
                for (k = 0; k < 3; k++) {
                    e1[k] = rotated[q[1]][k] - rotated[q[0]][k];
                    e2[k] = rotated[q[2]][k] - rotated[q[0]][k];
                }
                normal[0] = e1[1] * e2[2] - e1[2] * e2[1];
                normal[1] = e1[2] * e2[0] - e1[0] * e2[2];
                normal[2] = e1[0] * e2[1] - e1[1] * e2[0];
                len = sqrtf(normal[0] * normal[0] + normal[1] * normal[1] +
                            normal[2] * normal[2]);
                if (len > 0.0f) {
                    normal[0] /= len;
                    normal[1] /= len;
                    normal[2] /= len;
                }
            }
            dot = normal[0] * light[0] + normal[1] * light[1] +
                  normal[2] * light[2];
            if (dot < 0.0f) {
                dot = -dot; /* la cara mira al revés: se ilumina igual */
            }
            factor = 0.35f + 0.65f * dot;
            color = shade_color(SHAPE_FACE_COLOR[q[4]], factor);

            /* Dos triángulos por cara: (0,1,2) y (0,2,3). */
            for (k = 0; k < 2; k++) {
                gex_emit_triangle *tri = &tris[slot];
                int corners[3];
                int c;

                corners[0] = q[0];
                if (k == 0) {
                    corners[1] = q[1];
                    corners[2] = q[2];
                } else {
                    corners[1] = q[2];
                    corners[2] = q[3];
                }

                memset(tri, 0, sizeof(*tri));
                for (c = 0; c < 3; c++) {
                    const float *p = rotated[corners[c]];
                    tri->x[c] = (int16_t)(SHAPE_CENTER_X + p[0] * SHAPE_HALF_SIZE);
                    tri->y[c] = (int16_t)(SHAPE_CENTER_Y - p[1] * SHAPE_HALF_SIZE);
                    tri->z[c] = (int32_t)(SHAPE_Z_BASE + p[2] * SHAPE_Z_SCALE);
                    tri->color[c] = color;
                    tri->u[c] = 0;
                    tri->v[c] = 0;
                }
                tri->texture = 0;
                tri->layer = 0;
                tri->flags = GEX_EMIT_F_NEAR_EMITTER;
                tri->source_poly = (uint32_t)face;
                tri->seq = (uint32_t)slot;
                slot++;
            }
        }
    }

    return 0;
}
