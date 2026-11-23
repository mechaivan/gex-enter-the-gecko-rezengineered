/*
 * test_comp2d.c - compositor 2D portable (S-13).
 *
 * Fija las constantes **medidas** de cada ruta 2D para que un cambio
 * accidental no pase inadvertido:
 *
 *   - rectángulo `0xF0`: 2 triángulos, sin textura, z = 0;
 *   - cuadrilátero `0x2C`: UV de bytes tal cual y textura de 14 bits;
 *   - glifo `0x00432BBD`: caja con el `+3` del original y `w = (wh & 0xFF)<<4`,
 *     `h = (wh >> 8) * 12`;
 *   - línea `0x40`: banda de 1 px en las filas/columnas del segmento;
 *   - degradado por bandas: fronteras exactas y color interpolado entre
 *     paradas.
 *
 * No lee ningún fichero: son comprobaciones de aritmética y de emisión.
 */

#include <stdio.h>
#include <string.h>

#include "gex/comp2d.h"
#include "gex/emit.h"

static int failures;
static int checks;

#define CHECK(cond)                                                            \
    do {                                                                       \
        checks++;                                                              \
        if (!(cond)) {                                                         \
            failures++;                                                        \
            printf("FAIL %s:%d: %s\n", __FILE__, __LINE__, #cond);             \
        }                                                                      \
    } while (0)

#define CHECK_EQ_I(a, b)                                                       \
    do {                                                                       \
        long long va = (long long)(a);                                         \
        long long vb = (long long)(b);                                         \
        checks++;                                                              \
        if (va != vb) {                                                        \
            failures++;                                                        \
            printf("FAIL %s:%d: %s = %lld, esperado %lld\n", __FILE__,         \
                   __LINE__, #a, va, vb);                                      \
        }                                                                      \
    } while (0)

/* ------------------------------------------------------------------ */

static void test_rect(void) {
    gex_emit_output out;
    gex_comp2d_box box;
    gex_emit_status st;

    memset(&out, 0, sizeof(out));
    box.x = 10;
    box.y = 20;
    box.w = 100;
    box.h = 50;
    st = gex_comp2d_rect(&out, box, 0xFF102030u, GEX_BLEND_OPAQUE);
    CHECK_EQ_I(st, GEX_EMIT_OK);
    CHECK_EQ_I(out.count, 2);
    if (out.count == 2) {
        const gex_emit_triangle *a = &out.tris[0];
        const gex_emit_triangle *b = &out.tris[1];
        CHECK_EQ_I(a->texture, GEX_COMP2D_NO_TEXTURE);
        CHECK_EQ_I(a->opcode, 0xF0);
        CHECK_EQ_I(a->blend, GEX_BLEND_OPAQUE);
        CHECK_EQ_I(a->texture_mode, 0);
        CHECK_EQ_I(a->z[0], 0);
        CHECK_EQ_I(a->z[1], 0);
        CHECK_EQ_I(a->z[2], 0);
        CHECK_EQ_I(a->source_poly, 0xFFFFFFFFu);
        CHECK_EQ_I(a->color[0], 0xFF102030u);
        CHECK_EQ_I(b->color[2], 0xFF102030u);
        /* Triángulo 0: (10,20) (110,20) (110,70). */
        CHECK_EQ_I(a->x[0], 10);
        CHECK_EQ_I(a->y[0], 20);
        CHECK_EQ_I(a->x[1], 110);
        CHECK_EQ_I(a->y[1], 20);
        CHECK_EQ_I(a->x[2], 110);
        CHECK_EQ_I(a->y[2], 70);
        /* Triángulo 1: (10,20) (110,70) (10,70). */
        CHECK_EQ_I(b->x[0], 10);
        CHECK_EQ_I(b->y[0], 20);
        CHECK_EQ_I(b->x[1], 110);
        CHECK_EQ_I(b->y[1], 70);
        CHECK_EQ_I(b->x[2], 10);
        CHECK_EQ_I(b->y[2], 70);
        /* `seq` lo asigna el emisor y es creciente. */
        CHECK(a->seq < b->seq);
    }

    /* Área vacía: no emite nada y no es un error. */
    out.count = 0;
    box.w = 0;
    st = gex_comp2d_rect(&out, box, 0xFFFFFFFFu, GEX_BLEND_ALPHA);
    CHECK_EQ_I(st, GEX_EMIT_OK);
    CHECK_EQ_I(out.count, 0);
    box.w = 4;
    box.h = -1;
    st = gex_comp2d_rect(&out, box, 0xFFFFFFFFu, GEX_BLEND_ALPHA);
    CHECK_EQ_I(st, GEX_EMIT_OK);
    CHECK_EQ_I(out.count, 0);

    /* Puntero nulo: error explícito. */
    CHECK_EQ_I(gex_comp2d_rect(NULL, box, 0xFFFFFFFFu, GEX_BLEND_ALPHA),
               GEX_EMIT_ERR_ARG);
    gex_emit_output_free(&out);
}

static void test_quad(void) {
    gex_emit_output out;
    int16_t xy[4][2] = {{0, 0}, {16, 0}, {16, 16}, {0, 16}};
    uint8_t uv[4][2] = {{0, 0}, {255, 0}, {255, 255}, {0, 255}};
    uint32_t argb[4] = {0xFFFFFFFFu, 0xFFFF0000u, 0xFF00FF00u, 0xFF0000FFu};
    gex_emit_status st;

    memset(&out, 0, sizeof(out));
    st = gex_comp2d_quad(&out, xy, uv, 0x4123u, argb, GEX_BLEND_ALPHA);
    CHECK_EQ_I(st, GEX_EMIT_OK);
    CHECK_EQ_I(out.count, 2);
    if (out.count == 2) {
        const gex_emit_triangle *a = &out.tris[0];
        /* El índice se recorta a 14 bits (el bit 14 es el modo). */
        CHECK_EQ_I(a->texture, 0x0123);
        CHECK_EQ_I(a->opcode, 0x2C);
        CHECK_EQ_I(a->texture_mode, 1);
        CHECK_EQ_I(a->u[0], 0);
        CHECK_EQ_I(a->v[0], 0);
        CHECK_EQ_I(a->u[1], 255);
        CHECK_EQ_I(a->v[2], 255);
        CHECK_EQ_I(a->color[0], 0xFFFFFFFFu);
        CHECK_EQ_I(a->color[1], 0xFFFF0000u);
        CHECK_EQ_I(a->color[2], 0xFF00FF00u);
        CHECK_EQ_I(out.tris[1].color[2], 0xFF0000FFu);
    }
    CHECK_EQ_I(gex_comp2d_quad(NULL, xy, uv, 1, argb, 0), GEX_EMIT_ERR_ARG);
    CHECK_EQ_I(gex_comp2d_quad(&out, NULL, uv, 1, argb, 0), GEX_EMIT_ERR_ARG);
    gex_emit_output_free(&out);
}

static void test_tiles(void) {
    gex_emit_output out;
    gex_comp2d_tile tiles[3];
    gex_emit_status st;

    memset(&out, 0, sizeof(out));
    tiles[0].x = 0;
    tiles[0].y = 0;
    tiles[0].texture = 325;
    tiles[0].width = 16;
    tiles[0].height = 16;
    tiles[1].x = 16;
    tiles[1].y = 0;
    tiles[1].texture = 326;
    tiles[1].width = 16;
    tiles[1].height = 16;
    tiles[2].x = 32;
    tiles[2].y = 0;
    tiles[2].texture = 358;
    tiles[2].width = 32;
    tiles[2].height = 32;
    st = gex_comp2d_tiles(&out, tiles, 3, 0xFFFFFFFFu, GEX_BLEND_OPAQUE);
    CHECK_EQ_I(st, GEX_EMIT_OK);
    CHECK_EQ_I(out.count, 6);
    if (out.count == 6) {
        CHECK_EQ_I(out.tris[0].texture, 325);
        CHECK_EQ_I(out.tris[2].texture, 326);
        CHECK_EQ_I(out.tris[4].texture, 358);
        /* El tile 32x32 va de (32,0) a (64,32). */
        CHECK_EQ_I(out.tris[4].x[1], 64);
        CHECK_EQ_I(out.tris[4].y[2], 32);
        /* UV de la textura entera. */
        CHECK_EQ_I(out.tris[4].u[0], 0);
        CHECK_EQ_I(out.tris[4].u[2], 255);
        CHECK_EQ_I(out.tris[4].v[2], 255);
    }

    /* Tile de tamaño cero: se omite sin error. */
    out.count = 0;
    tiles[0].width = 0;
    st = gex_comp2d_tiles(&out, tiles, 1, 0xFFFFFFFFu, GEX_BLEND_OPAQUE);
    CHECK_EQ_I(st, GEX_EMIT_OK);
    CHECK_EQ_I(out.count, 0);
    CHECK_EQ_I(gex_comp2d_tiles(NULL, tiles, 1, 0, 0), GEX_EMIT_ERR_ARG);
    CHECK_EQ_I(gex_comp2d_tiles(&out, NULL, 1, 0, 0), GEX_EMIT_ERR_ARG);
    gex_emit_output_free(&out);
}

static void test_glyph(void) {
    gex_comp2d_box box;
    gex_emit_output out;
    uint8_t uv[4][2] = {{0, 0}, {255, 0}, {255, 255}, {0, 255}};
    gex_emit_status st;

    /* xy = 0x1234 -> x = 0x34, y = 0x12 + 3 = 0x15; wh = 0x0201 -> 16 x 24. */
    box = gex_comp2d_glyph_box(0x1234u, 0x0201u);
    CHECK_EQ_I(box.x, 0x34);
    CHECK_EQ_I(box.y, 0x15);
    CHECK_EQ_I(box.w, 16);
    CHECK_EQ_I(box.h, 24);

    /* wh = 0x0100 -> ancho 0: el original no dibuja. */
    box = gex_comp2d_glyph_box(0x0000u, 0x0100u);
    CHECK_EQ_I(box.w, 0);
    CHECK_EQ_I(box.h, 12);

    memset(&out, 0, sizeof(out));
    st = gex_comp2d_glyph(&out, 0x1234u, 0x0201u, uv, 500, 0xFFFFFFFFu,
                          GEX_BLEND_ALPHA);
    CHECK_EQ_I(st, GEX_EMIT_OK);
    CHECK_EQ_I(out.count, 2);
    if (out.count == 2) {
        CHECK_EQ_I(out.tris[0].x[0], 0x34);
        CHECK_EQ_I(out.tris[0].y[0], 0x15);
        CHECK_EQ_I(out.tris[0].x[1], 0x44);
        CHECK_EQ_I(out.tris[0].y[2], 0x15 + 24);
        CHECK_EQ_I(out.tris[0].texture, 500);
    }

    /* Glifo de ancho cero: sin triángulos. */
    out.count = 0;
    st = gex_comp2d_glyph(&out, 0, 0x0100u, uv, 500, 0xFFFFFFFFu,
                          GEX_BLEND_ALPHA);
    CHECK_EQ_I(st, GEX_EMIT_OK);
    CHECK_EQ_I(out.count, 0);
    CHECK_EQ_I(gex_comp2d_glyph(NULL, 0, 0x0101u, uv, 1, 0, 0),
               GEX_EMIT_ERR_ARG);
    gex_emit_output_free(&out);
}

static void test_line(void) {
    gex_emit_output out;
    gex_emit_status st;

    memset(&out, 0, sizeof(out));
    /* Horizontal de 1 px: filas y0..y0+1. */
    st = gex_comp2d_line(&out, 10, 10, 110, 10, 1, 0xFFFFFFFFu,
                         GEX_BLEND_ALPHA);
    CHECK_EQ_I(st, GEX_EMIT_OK);
    CHECK_EQ_I(out.count, 2);
    if (out.count == 2) {
        CHECK_EQ_I(out.tris[0].opcode, 0x40);
        CHECK_EQ_I(out.tris[0].texture, GEX_COMP2D_NO_TEXTURE);
        CHECK_EQ_I(out.tris[0].y[0], 10);
        CHECK_EQ_I(out.tris[0].y[2], 11);
        CHECK_EQ_I(out.tris[0].x[0], 10);
        CHECK_EQ_I(out.tris[0].x[1], 110);
    }

    /* Vertical de 3 px: columnas x0..x0+3. */
    out.count = 0;
    st = gex_comp2d_line(&out, 20, 5, 20, 105, 3, 0xFFFFFFFFu,
                         GEX_BLEND_ALPHA);
    CHECK_EQ_I(st, GEX_EMIT_OK);
    if (out.count == 2) {
        CHECK_EQ_I(out.tris[0].x[0], 20);
        CHECK_EQ_I(out.tris[0].x[1], 23);
        CHECK_EQ_I(out.tris[0].y[0], 5);
        CHECK_EQ_I(out.tris[0].y[2], 105);
    }

    /* Segmento de longitud cero: no emite. */
    out.count = 0;
    st = gex_comp2d_line(&out, 5, 5, 5, 5, 1, 0xFFFFFFFFu, GEX_BLEND_ALPHA);
    CHECK_EQ_I(st, GEX_EMIT_OK);
    CHECK_EQ_I(out.count, 0);

    /* Diagonal: la banda tiene grosor. */
    out.count = 0;
    st = gex_comp2d_line(&out, 0, 0, 100, 100, 2, 0xFFFFFFFFu,
                         GEX_BLEND_ALPHA);
    CHECK_EQ_I(st, GEX_EMIT_OK);
    CHECK_EQ_I(out.count, 2);
    CHECK_EQ_I(gex_comp2d_line(NULL, 0, 0, 1, 1, 1, 0, 0), GEX_EMIT_ERR_ARG);
    gex_emit_output_free(&out);
}

static void test_gradient(void) {
    gex_emit_output out;
    gex_comp2d_stop stops[2];
    gex_comp2d_box box;
    gex_emit_status st;

    stops[0].y = 0;
    stops[0].r = 0;
    stops[0].g = 0;
    stops[0].b = 0;
    stops[1].y = 383;
    stops[1].r = 255;
    stops[1].g = 255;
    stops[1].b = 255;
    box.x = 0;
    box.y = 0;
    box.w = 512;
    box.h = 384;

    memset(&out, 0, sizeof(out));
    st = gex_comp2d_vertical_gradient(&out, box, stops, 2, 2);
    CHECK_EQ_I(st, GEX_EMIT_OK);
    CHECK_EQ_I(out.count, 4);
    if (out.count == 4) {
        /* Banda 0: y 0..192, color en y = 96 -> 96/383 = 0,2506 -> 64. */
        CHECK_EQ_I(out.tris[0].y[0], 0);
        CHECK_EQ_I(out.tris[0].y[2], 192);
        CHECK_EQ_I((out.tris[0].color[0] >> 16) & 0xFFu, 64);
        CHECK_EQ_I(out.tris[0].color[0] >> 24, 0xFF);
        /* Banda 1: y 192..384, color en y = 288 -> 288/383 = 0,7519 -> 192. */
        CHECK_EQ_I(out.tris[2].y[0], 192);
        CHECK_EQ_I(out.tris[2].y[2], 384);
        CHECK_EQ_I((out.tris[2].color[0] >> 16) & 0xFFu, 192);
        /* Sin huecos: la banda 0 acaba donde empieza la 1. */
        CHECK_EQ_I(out.tris[0].y[2], out.tris[2].y[0]);
    }

    /* Una sola parada: todas las bandas con ese color. */
    out.count = 0;
    st = gex_comp2d_vertical_gradient(&out, box, stops, 1, 3);
    CHECK_EQ_I(st, GEX_EMIT_OK);
    CHECK_EQ_I(out.count, 6);
    if (out.count == 6) {
        CHECK_EQ_I(out.tris[0].color[0], 0xFF000000u);
        CHECK_EQ_I(out.tris[4].color[0], 0xFF000000u);
    }

    /* Paradas desordenadas: se busca la más cercana por arriba y por abajo. */
    out.count = 0;
    {
        gex_comp2d_stop unsorted[2];
        unsorted[0] = stops[1];
        unsorted[1] = stops[0];
        st = gex_comp2d_vertical_gradient(&out, box, unsorted, 2, 1);
        CHECK_EQ_I(st, GEX_EMIT_OK);
        CHECK_EQ_I(out.count, 2);
        if (out.count == 2) {
            CHECK_EQ_I((out.tris[0].color[0] >> 16) & 0xFFu, 128);
        }
    }

    /* Argumentos inválidos. */
    CHECK_EQ_I(gex_comp2d_vertical_gradient(NULL, box, stops, 2, 2),
               GEX_EMIT_ERR_ARG);
    CHECK_EQ_I(gex_comp2d_vertical_gradient(&out, box, NULL, 2, 2),
               GEX_EMIT_ERR_ARG);
    CHECK_EQ_I(gex_comp2d_vertical_gradient(&out, box, stops, 0, 2),
               GEX_EMIT_ERR_ARG);
    CHECK_EQ_I(gex_comp2d_vertical_gradient(&out, box, stops, 2, 0),
               GEX_EMIT_ERR_ARG);
    gex_emit_output_free(&out);
}

/* ------------------------------------------------------------------ */
/* Copia y prepend del emisor                                         */
/* ------------------------------------------------------------------ */

static void push_marker(gex_emit_output *out, uint32_t color) {
    gex_emit_triangle tri;
    int c;

    memset(&tri, 0, sizeof(tri));
    for (c = 0; c < 3; c++) {
        tri.color[c] = color;
    }
    tri.opcode = 0x99;
    tri.texture = GEX_COMP2D_NO_TEXTURE;
    tri.x[0] = (int16_t)color;
    CHECK(gex_emit_output_push(out, &tri) == GEX_EMIT_OK);
}

static void test_copy_prepend(void) {
    gex_emit_output a, b, c;
    size_t i;

    memset(&a, 0, sizeof(a));
    memset(&b, 0, sizeof(b));
    memset(&c, 0, sizeof(c));

    /* Copia vac�a: no reserva nada. */
    CHECK(gex_emit_output_copy(&a, &b) == GEX_EMIT_OK);
    CHECK(a.count == 0);

    push_marker(&a, 0x10u);
    push_marker(&a, 0x20u);
    push_marker(&a, 0x30u);
    CHECK(a.count == 3);

    /* La copia es profunda e independiente. */
    CHECK(gex_emit_output_copy(&b, &a) == GEX_EMIT_OK);
    CHECK(b.count == 3);
    CHECK(b.capacity >= 3);
    a.tris[0].color[0] = 0xFFFFFFFFu;
    CHECK(b.tris[0].color[0] == 0x10u);
    for (i = 0; i < 3; i++) {
        CHECK(b.tris[i].seq == i);
    }
    CHECK(gex_emit_output_copy(&b, &b) == GEX_EMIT_ERR_ARG);
    CHECK(gex_emit_output_copy(NULL, &a) == GEX_EMIT_ERR_ARG);

    /* Prepend: `a` delante de `b`, sin reordenar. */
    CHECK(gex_emit_output_prepend(&b, &a) == GEX_EMIT_OK);
    CHECK(b.count == 6);
    CHECK(b.tris[0].color[0] == 0xFFFFFFFFu); /* la de `a`, modificada */
    CHECK(b.tris[1].color[0] == 0x20u);
    CHECK(b.tris[2].color[0] == 0x30u);
    CHECK(b.tris[3].color[0] == 0x10u); /* la copia de `a` */
    CHECK(b.tris[4].color[0] == 0x20u);
    CHECK(b.tris[5].color[0] == 0x30u);
    for (i = 0; i < 6; i++) {
        CHECK(b.tris[i].seq == i);
    }

    /* Prepend vac�o y errores. */
    CHECK(gex_emit_output_prepend(&c, &c) == GEX_EMIT_ERR_ARG);
    CHECK(gex_emit_output_prepend(&c, NULL) == GEX_EMIT_ERR_ARG);
    CHECK(gex_emit_output_prepend(NULL, &a) == GEX_EMIT_ERR_ARG);
    {
        gex_emit_output empty;
        memset(&empty, 0, sizeof(empty));
        CHECK(gex_emit_output_prepend(&c, &empty) == GEX_EMIT_OK);
        CHECK(c.count == 0);
    }

    gex_emit_output_free(&a);
    gex_emit_output_free(&b);
    gex_emit_output_free(&c);
}

/* ------------------------------------------------------------------ */

int main(void) {
    test_rect();
    test_quad();
    test_tiles();
    test_glyph();
    test_line();
    test_gradient();
    test_copy_prepend();

    if (failures != 0) {
        printf("%d fallos de %d comprobaciones\n", failures, checks);
        return 1;
    }
    printf("%d comprobaciones, 0 fallos\n", checks);
    return 0;
}
