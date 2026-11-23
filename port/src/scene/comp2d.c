/*
 * comp2d.c - compositor 2D portable (S-13). Véase gex/comp2d.h.
 *
 * Todas las rutinas construyen `gex_emit_triangle` en píxeles lógicos y las
 * empujan con `gex_emit_output_push`, así que comparten el crecimiento, el
 * contador `seq` y la contabilidad del emisor. Ninguna toca el atlas, el
 * shader ni el estado de mezcla: eso lo decide quien llama con `blend`.
 *
 * Las constantes de cada primitiva están tomadas de la reconstrucción estática
 * de S-13 (docs/D3D11_FIRST_SCENE_PLAN.md § 29) y de los resultados U1F/U1H;
 * donde el original no está descodificado se dice en el comentario.
 */

#include "gex/comp2d.h"

#include <math.h>
#include <string.h>

/* ------------------------------------------------------------------ */
/* Empuje de un triángulo de pantalla                                  */
/* ------------------------------------------------------------------ */

static gex_emit_status push_screen_triangle(gex_emit_output *out,
                                            const int16_t x[3],
                                            const int16_t y[3],
                                            const uint8_t u[3],
                                            const uint8_t v[3],
                                            const uint32_t color[3],
                                            uint16_t texture, uint8_t opcode,
                                            uint8_t blend) {
    gex_emit_triangle tri;
    int i;

    if (out == NULL) {
        return GEX_EMIT_ERR_ARG;
    }
    memset(&tri, 0, sizeof(tri));
    for (i = 0; i < 3; i++) {
        tri.x[i] = x[i];
        tri.y[i] = y[i];
        tri.z[i] = 0; /* sin depth buffer: el orden del array es el orden */
        tri.u[i] = u[i];
        tri.v[i] = v[i];
        tri.color[i] = color[i];
    }
    tri.texture = texture;
    tri.layer = 0;
    tri.texture_mode = (texture == GEX_COMP2D_NO_TEXTURE) ? 0u : 1u;
    tri.opcode = opcode;
    tri.blend = blend;
    tri.flags = 0;
    /* Los comandos 2D no vienen de un polígono del nivel: se marcan para que
     * una traza pueda separarlos de la geometría. */
    tri.source_poly = 0xFFFFFFFFu;
    return gex_emit_output_push(out, &tri);
}

/* Dos triángulos a partir de cuatro esquinas (orden 0-1-2 y 0-2-3). */
static gex_emit_status push_quad(gex_emit_output *out, const int16_t xy[4][2],
                                 const uint8_t uv[4][2], const uint32_t argb[4],
                                 uint16_t texture, uint8_t opcode,
                                 uint8_t blend) {
    static const int kTri[2][3] = {{0, 1, 2}, {0, 2, 3}};
    int t;

    for (t = 0; t < 2; t++) {
        int16_t x[3], y[3];
        uint8_t u[3], v[3];
        uint32_t color[3];
        gex_emit_status st;
        int c;

        for (c = 0; c < 3; c++) {
            int k = kTri[t][c];
            x[c] = xy[k][0];
            y[c] = xy[k][1];
            u[c] = uv[k][0];
            v[c] = uv[k][1];
            color[c] = argb[k];
        }
        st = push_screen_triangle(out, x, y, u, v, color, texture, opcode,
                                  blend);
        if (st != GEX_EMIT_OK) {
            return st;
        }
    }
    return GEX_EMIT_OK;
}

/* ------------------------------------------------------------------ */
/* Rectángulo sólido (`0xF0`, handler `0x00469268`)                    */
/* ------------------------------------------------------------------ */

gex_emit_status gex_comp2d_rect(gex_emit_output *out, gex_comp2d_box box,
                                uint32_t argb, uint8_t blend) {
    int16_t xy[4][2];
    uint8_t uv[4][2];
    uint32_t color[4];
    int i;

    if (out == NULL) {
        return GEX_EMIT_ERR_ARG;
    }
    if (box.w <= 0 || box.h <= 0) {
        return GEX_EMIT_OK; /* área vacía: el original tampoco dibuja nada */
    }
    xy[0][0] = box.x;
    xy[0][1] = box.y;
    xy[1][0] = (int16_t)(box.x + box.w);
    xy[1][1] = box.y;
    xy[2][0] = (int16_t)(box.x + box.w);
    xy[2][1] = (int16_t)(box.y + box.h);
    xy[3][0] = box.x;
    xy[3][1] = (int16_t)(box.y + box.h);
    for (i = 0; i < 4; i++) {
        uv[i][0] = 0;
        uv[i][1] = 0;
        color[i] = argb;
    }
    return push_quad(out, xy, uv, color, GEX_COMP2D_NO_TEXTURE, 0xF0u, blend);
}

/* ------------------------------------------------------------------ */
/* Cuadrilátero texturado (`0x2C` / `0x24`)                            */
/* ------------------------------------------------------------------ */

gex_emit_status gex_comp2d_quad(gex_emit_output *out, const int16_t xy[4][2],
                                const uint8_t uv[4][2], uint16_t texture,
                                const uint32_t argb[4], uint8_t blend) {
    if (out == NULL || xy == NULL || uv == NULL || argb == NULL) {
        return GEX_EMIT_ERR_ARG;
    }
    return push_quad(out, xy, uv, argb, (uint16_t)(texture & 0x3FFFu), 0x2Cu,
                     blend);
}

/* ------------------------------------------------------------------ */
/* Capa de tiles                                                       */
/* ------------------------------------------------------------------ */

gex_emit_status gex_comp2d_tiles(gex_emit_output *out,
                                 const gex_comp2d_tile *tiles, size_t count,
                                 uint32_t argb, uint8_t blend) {
    size_t i;

    if (out == NULL || (tiles == NULL && count != 0)) {
        return GEX_EMIT_ERR_ARG;
    }
    for (i = 0; i < count; i++) {
        const gex_comp2d_tile *t = &tiles[i];
        gex_comp2d_box box;
        gex_emit_status st;

        if (t->width == 0 || t->height == 0) {
            continue;
        }
        box.x = t->x;
        box.y = t->y;
        box.w = (int16_t)t->width;
        box.h = (int16_t)t->height;
        /* Un tile es la textura entera: UV 0 y 255 en las cuatro esquinas. */
        {
            int16_t xy[4][2];
            uint8_t uv[4][2];
            uint32_t color[4];
            int k;

            xy[0][0] = box.x;
            xy[0][1] = box.y;
            xy[1][0] = (int16_t)(box.x + box.w);
            xy[1][1] = box.y;
            xy[2][0] = (int16_t)(box.x + box.w);
            xy[2][1] = (int16_t)(box.y + box.h);
            xy[3][0] = box.x;
            xy[3][1] = (int16_t)(box.y + box.h);
            uv[0][0] = 0;
            uv[0][1] = 0;
            uv[1][0] = 255;
            uv[1][1] = 0;
            uv[2][0] = 255;
            uv[2][1] = 255;
            uv[3][0] = 0;
            uv[3][1] = 255;
            for (k = 0; k < 4; k++) {
                color[k] = argb;
            }
            st = push_quad(out, xy, uv, color,
                           (uint16_t)(t->texture & 0x3FFFu), 0x2Cu, blend);
            if (st != GEX_EMIT_OK) {
                return st;
            }
        }
    }
    return GEX_EMIT_OK;
}

/* ------------------------------------------------------------------ */
/* Glifo (`0x00432BBD`, FONT.3DF)                                      */
/* ------------------------------------------------------------------ */

gex_comp2d_box gex_comp2d_glyph_box(uint16_t glyph_xy, uint16_t glyph_wh) {
    gex_comp2d_box box;

    box.x = (int16_t)(glyph_xy & 0x00FFu);
    box.y = (int16_t)((glyph_xy >> 8) + 3u); /* el +3 es del original */
    box.w = (int16_t)((glyph_wh & 0x00FFu) << 4);
    box.h = (int16_t)((glyph_wh >> 8) * 12u);
    return box;
}

gex_emit_status gex_comp2d_glyph(gex_emit_output *out, uint16_t glyph_xy,
                                 uint16_t glyph_wh, const uint8_t uv[4][2],
                                 uint16_t texture, uint32_t argb,
                                 uint8_t blend) {
    gex_comp2d_box box;
    int16_t xy[4][2];
    uint32_t color[4];
    int i;

    if (out == NULL || uv == NULL) {
        return GEX_EMIT_ERR_ARG;
    }
    box = gex_comp2d_glyph_box(glyph_xy, glyph_wh);
    if (box.w <= 0 || box.h <= 0) {
        return GEX_EMIT_OK;
    }
    xy[0][0] = box.x;
    xy[0][1] = box.y;
    xy[1][0] = (int16_t)(box.x + box.w);
    xy[1][1] = box.y;
    xy[2][0] = (int16_t)(box.x + box.w);
    xy[2][1] = (int16_t)(box.y + box.h);
    xy[3][0] = box.x;
    xy[3][1] = (int16_t)(box.y + box.h);
    for (i = 0; i < 4; i++) {
        color[i] = argb;
    }
    return push_quad(out, xy, uv, color, (uint16_t)(texture & 0x3FFFu), 0x2Cu,
                     blend);
}

/* ------------------------------------------------------------------ */
/* Línea / primitiva 2D (`0x40`, `0x004FF06F`)                         */
/* ------------------------------------------------------------------ */

gex_emit_status gex_comp2d_line(gex_emit_output *out, int16_t x0, int16_t y0,
                                int16_t x1, int16_t y1, int16_t thickness,
                                uint32_t argb, uint8_t blend) {
    double dx, dy, len, nx, ny;
    int16_t xy[4][2];
    uint8_t uv[4][2];
    uint32_t color[4];
    int i;

    if (out == NULL) {
        return GEX_EMIT_ERR_ARG;
    }
    if (thickness <= 0) {
        thickness = 1;
    }
    dx = (double)x1 - (double)x0;
    dy = (double)y1 - (double)y0;
    len = sqrt(dx * dx + dy * dy);
    if (len == 0.0) {
        return GEX_EMIT_OK;
    }
    if (dy == 0.0) {
        /* Horizontal: la banda ocupa las filas y0 .. y0 + grosor, que es
         * donde cae el píxel de `grDrawLine` para grosor 1. */
        xy[0][0] = (x0 <= x1) ? x0 : x1;
        xy[0][1] = y0;
        xy[1][0] = (x0 <= x1) ? x1 : x0;
        xy[1][1] = y0;
        xy[2][0] = xy[1][0];
        xy[2][1] = (int16_t)(y0 + thickness);
        xy[3][0] = xy[0][0];
        xy[3][1] = xy[2][1];
    } else if (dx == 0.0) {
        /* Vertical: banda de columnas x0 .. x0 + grosor. */
        xy[0][0] = x0;
        xy[0][1] = (y0 <= y1) ? y0 : y1;
        xy[1][0] = (int16_t)(x0 + thickness);
        xy[1][1] = xy[0][1];
        xy[2][0] = xy[1][0];
        xy[2][1] = (y0 <= y1) ? y1 : y0;
        xy[3][0] = x0;
        xy[3][1] = xy[2][1];
    } else {
        /* Diagonal: media anchura perpendicular, centrada en el segmento. */
        nx = -dy / len * (double)thickness * 0.5;
        ny = dx / len * (double)thickness * 0.5;
        xy[0][0] = (int16_t)lround((double)x0 + nx);
        xy[0][1] = (int16_t)lround((double)y0 + ny);
        xy[1][0] = (int16_t)lround((double)x1 + nx);
        xy[1][1] = (int16_t)lround((double)y1 + ny);
        xy[2][0] = (int16_t)lround((double)x1 - nx);
        xy[2][1] = (int16_t)lround((double)y1 - ny);
        xy[3][0] = (int16_t)lround((double)x0 - nx);
        xy[3][1] = (int16_t)lround((double)y0 - ny);
    }
    for (i = 0; i < 4; i++) {
        uv[i][0] = 0;
        uv[i][1] = 0;
        color[i] = argb;
    }
    return push_quad(out, xy, uv, color, GEX_COMP2D_NO_TEXTURE, 0x40u, blend);
}

/* ------------------------------------------------------------------ */
/* Degradado por bandas                                                */
/* ------------------------------------------------------------------ */

static void stop_color_at(const gex_comp2d_stop *stops, size_t count, int y,
                          uint8_t rgb[3]) {
    size_t i;
    const gex_comp2d_stop *lo = NULL;
    const gex_comp2d_stop *hi = NULL;

    /* Parada más cercana por debajo (`lo`) y por encima (`hi`). No se supone
     * que la lista esté ordenada ni que cubra la caja: si `y` queda fuera del
     * rango se usa la parada extrema de ese lado. */
    for (i = 0; i < count; i++) {
        if (stops[i].y <= y && (lo == NULL || stops[i].y > lo->y)) {
            lo = &stops[i];
        }
        if (stops[i].y >= y && (hi == NULL || stops[i].y < hi->y)) {
            hi = &stops[i];
        }
    }
    if (lo == NULL) {
        lo = hi;
    }
    if (hi == NULL) {
        hi = lo;
    }
    if (lo == hi || hi->y == lo->y) {
        rgb[0] = lo->r;
        rgb[1] = lo->g;
        rgb[2] = lo->b;
        return;
    }
    {
        double t = ((double)y - (double)lo->y) / ((double)hi->y - (double)lo->y);
        if (t < 0.0) {
            t = 0.0;
        }
        if (t > 1.0) {
            t = 1.0;
        }
        rgb[0] = (uint8_t)lround((double)lo->r + ((double)hi->r - lo->r) * t);
        rgb[1] = (uint8_t)lround((double)lo->g + ((double)hi->g - lo->g) * t);
        rgb[2] = (uint8_t)lround((double)lo->b + ((double)hi->b - lo->b) * t);
    }
}

gex_emit_status gex_comp2d_vertical_gradient(gex_emit_output *out,
                                             gex_comp2d_box box,
                                             const gex_comp2d_stop *stops,
                                             size_t stop_count,
                                             unsigned bands) {
    unsigned b;

    if (out == NULL || stops == NULL) {
        return GEX_EMIT_ERR_ARG;
    }
    if (stop_count == 0 || bands == 0) {
        return GEX_EMIT_ERR_ARG;
    }
    if (box.w <= 0 || box.h <= 0) {
        return GEX_EMIT_OK;
    }
    for (b = 0; b < bands; b++) {
        /* Fronteras enteras: la última banda termina exactamente en
         * box.y + box.h, sin huecos ni solapes. */
        int y0 = box.y + (int)((long)box.h * (long)b / (long)bands);
        int y1 = box.y + (int)((long)box.h * (long)(b + 1) / (long)bands);
        uint8_t rgb[3];
        uint32_t argb;
        gex_comp2d_box band;
        gex_emit_status st;

        if (y1 <= y0) {
            continue;
        }
        stop_color_at(stops, stop_count, (y0 + y1) / 2, rgb);
        argb = 0xFF000000u | ((uint32_t)rgb[0] << 16) |
               ((uint32_t)rgb[1] << 8) | (uint32_t)rgb[2];
        band.x = box.x;
        band.y = (int16_t)y0;
        band.w = box.w;
        band.h = (int16_t)(y1 - y0);
        st = gex_comp2d_rect(out, band, argb, GEX_BLEND_OPAQUE);
        if (st != GEX_EMIT_OK) {
            return st;
        }
    }
    return GEX_EMIT_OK;
}
