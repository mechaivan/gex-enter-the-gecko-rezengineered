/*
 * test_geometry.c - Módulo 3, fase portable: gex_emit_output -> vértices e
 * índices.
 *
 * Grupos:
 *   - un triángulo: conversión exacta de todos los atributos;
 *   - varios triángulos: índices y orden;
 *   - deduplicación exacta y determinista;
 *   - triángulos clipeados por profundidad (se saltan y se cuentan);
 *   - pasada de niebla: no se mezcla con el registro principal;
 *   - geometría subdividida real (gex_emit_subdivide);
 *   - extremo a extremo con una imagen DFX sintética (gex_level_emit);
 *   - entrada vacía, argumentos nulos, conteos incoherentes y overflow;
 *   - fallo de reserva inyectado (GEX_GEOM_TEST_FAULT_INJECT);
 *   - liberación repetida y estados a cero;
 *   - AZTEC16 real, si está disponible (solo lectura).
 */

#include "gex/geometry.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define AZTEC_PATH "C:/GEX_REZ/E2_A_ORIGINAL/LEVEL/AZTEC16.DFX"

static int checks;
static int failures;

#define CHECK(cond)                                                          \
    do {                                                                     \
        checks++;                                                            \
        if (!(cond)) {                                                       \
            failures++;                                                      \
            printf("FAIL %s:%d: %s\n", __FILE__, __LINE__, #cond);           \
        }                                                                    \
    } while (0)

/* ------------------------------------------------------------------ */
/* Constructores de entrada                                           */
/* ------------------------------------------------------------------ */

static gex_emit_triangle mk_tri(void)
{
    gex_emit_triangle t;
    memset(&t, 0, sizeof(t));
    t.x[0] = 10;  t.y[0] = 20;  t.z[0] = 1000;
    t.x[1] = 30;  t.y[1] = 20;  t.z[1] = 1100;
    t.x[2] = 10;  t.y[2] = 40;  t.z[2] = 1200;
    t.color[0] = 0xFF112233u;
    t.color[1] = 0x80445566u;
    t.color[2] = 0x00778899u;
    t.u[0] = 0;   t.v[0] = 255;
    t.u[1] = 128; t.v[1] = 64;
    t.u[2] = 255; t.v[2] = 1;
    t.texture = 0x0123u;
    t.layer = -3;
    t.flags = GEX_EMIT_F_NEAR_EMITTER;
    t.source_poly = 7;
    t.seq = 0;
    return t;
}

static gex_emit_output wrap(gex_emit_triangle *tris, size_t n)
{
    gex_emit_output o;
    memset(&o, 0, sizeof(o));
    o.tris = tris;
    o.count = n;
    o.capacity = n;
    return o;
}

/* ------------------------------------------------------------------ */
/* Un triángulo                                                       */
/* ------------------------------------------------------------------ */

static void test_single_triangle(void)
{
    gex_emit_triangle tri = mk_tri();
    gex_emit_output in = wrap(&tri, 1);
    gex_geometry g;

    CHECK(gex_geometry_build(&in, &g) == GEX_GEOM_OK);
    CHECK(g.vertex_count == 3u);
    CHECK(g.index_count == 3u);
    CHECK(g.triangle_count == 1u);
    CHECK(g.skipped_clipped == 0u);
    CHECK(g.vertices != NULL && g.indices != NULL);
    if (g.vertices == NULL || g.indices == NULL) {
        gex_geometry_free(&g);
        return;
    }

    CHECK(g.indices[0] == 0u && g.indices[1] == 1u && g.indices[2] == 2u);

    for (int i = 0; i < 3; i++) {
        const gex_vertex *v = &g.vertices[i];
        CHECK(v->x == (float)tri.x[i]);
        CHECK(v->y == (float)tri.y[i]);
        CHECK(v->z == (float)tri.z[i]);
        CHECK(v->color == tri.color[i]);
        CHECK(v->u == (float)tri.u[i] / 256.0f);
        CHECK(v->v == (float)tri.v[i] / 256.0f);
        CHECK(v->texture == tri.texture);
        CHECK(v->layer == tri.layer);
        CHECK(v->flags == tri.flags);
        /* Reversibilidad exacta de la UV normalizada. */
        CHECK((int)(v->u * 256.0f + 0.5f) == tri.u[i]);
        CHECK((int)(v->v * 256.0f + 0.5f) == tri.v[i]);
    }

    /* La deduplicación no comparte nada entre esquinas distintas. */
    CHECK(memcmp(&g.vertices[0], &g.vertices[1], sizeof(gex_vertex)) != 0);
    CHECK(memcmp(&g.vertices[1], &g.vertices[2], sizeof(gex_vertex)) != 0);

    gex_geometry_free(&g);
}

/* ------------------------------------------------------------------ */
/* Varios triángulos e índices                                        */
/* ------------------------------------------------------------------ */

static void test_multiple_triangles(void)
{
    gex_emit_triangle tris[3];
    gex_emit_output in;
    gex_geometry g;

    tris[0] = mk_tri();
    tris[1] = mk_tri();
    tris[1].x[0] += 100; /* distinto: no debe deduplicarse */
    tris[1].x[1] += 100;
    tris[1].x[2] += 100;
    tris[2] = tris[0];   /* idéntico a 0: debe compartir vértices */

    in = wrap(tris, 3);
    CHECK(gex_geometry_build(&in, &g) == GEX_GEOM_OK);
    /* 3 + 3 únicos (el tercero reutiliza los de tris[0]). */
    CHECK(g.vertex_count == 6u);
    CHECK(g.index_count == 9u);
    CHECK(g.triangle_count == 3u);
    if (g.indices != NULL) {
        const uint32_t expect[9] = {0, 1, 2, 3, 4, 5, 0, 1, 2};
        for (int i = 0; i < 9; i++) {
            CHECK(g.indices[i] == expect[i]);
        }
        for (size_t i = 0; i < g.index_count; i++) {
            CHECK(g.indices[i] < g.vertex_count);
        }
    }
    gex_geometry_free(&g);

    /* El tercero solo cambia el color de su primera esquina: se reutilizan
     * las otras dos, así que hay 3 + 3 + 1 = 7 vértices únicos. */
    tris[2] = tris[0];
    tris[2].color[0] ^= 0x00000001u;
    in = wrap(tris, 3);
    CHECK(gex_geometry_build(&in, &g) == GEX_GEOM_OK);
    CHECK(g.vertex_count == 7u);
    CHECK(g.index_count == 9u);
    gex_geometry_free(&g);
}

/* ------------------------------------------------------------------ */
/* Clipeados por profundidad                                          */
/* ------------------------------------------------------------------ */

static void test_clipped_skipped(void)
{
    gex_emit_triangle tris[3];
    gex_emit_output in;
    gex_geometry g;

    tris[0] = mk_tri();
    tris[0].flags |= GEX_EMIT_F_DEPTH_CLIPPED;
    tris[0].x[0] = tris[0].x[1] = tris[0].x[2] = 0;
    tris[0].y[0] = tris[0].y[1] = tris[0].y[2] = 0;
    tris[1] = mk_tri();
    tris[2] = mk_tri();
    tris[2].x[0] += 50;

    in = wrap(tris, 3);
    CHECK(gex_geometry_build(&in, &g) == GEX_GEOM_OK);
    CHECK(g.skipped_clipped == 1u);
    CHECK(g.triangle_count == 2u);
    CHECK(g.index_count == 6u);
    /* El segundo triángulo solo cambia su primera esquina: 3 + 1 = 4. */
    CHECK(g.vertex_count == 4u);
    if (g.vertices != NULL) {
        /* Ningún vértice con x/y a cero (los del clipeado no entraron). */
        for (size_t i = 0; i < g.vertex_count; i++) {
            CHECK(g.vertices[i].x != 0.0f || g.vertices[i].y != 0.0f);
        }
    }
    gex_geometry_free(&g);

    /* Todo clipeado: geometría vacía válida, sin reservas. */
    for (int i = 0; i < 3; i++) {
        tris[i] = mk_tri();
        tris[i].flags |= GEX_EMIT_F_DEPTH_CLIPPED;
    }
    in = wrap(tris, 3);
    CHECK(gex_geometry_build(&in, &g) == GEX_GEOM_OK);
    CHECK(g.vertex_count == 0u);
    CHECK(g.index_count == 0u);
    CHECK(g.triangle_count == 0u);
    CHECK(g.skipped_clipped == 3u);
    CHECK(g.vertices == NULL && g.indices == NULL);
    gex_geometry_free(&g);
}

/* ------------------------------------------------------------------ */
/* Pasada de niebla                                                   */
/* ------------------------------------------------------------------ */

static void test_fog_pass(void)
{
    gex_emit_triangle tris[2];
    gex_emit_output in;
    gex_geometry g;

    /* Mismo triángulo, colores distintos y flag FOG_PASS: no se fusionan. */
    tris[0] = mk_tri();
    tris[0].flags = GEX_EMIT_F_FAR_EMITTER;
    tris[1] = tris[0];
    tris[1].flags = GEX_EMIT_F_FOG_PASS | GEX_EMIT_F_FAR_EMITTER;
    for (int i = 0; i < 3; i++) {
        tris[1].color[i] = 0x00102030u + (uint32_t)i;
    }

    in = wrap(tris, 2);
    CHECK(gex_geometry_build(&in, &g) == GEX_GEOM_OK);
    CHECK(g.vertex_count == 6u);
    CHECK(g.index_count == 6u);
    CHECK(g.triangle_count == 2u);
    if (g.vertices != NULL && g.indices != NULL) {
        for (int i = 0; i < 3; i++) {
            const gex_vertex *main_v = &g.vertices[g.indices[i]];
            const gex_vertex *fog_v = &g.vertices[g.indices[3 + i]];
            CHECK((main_v->flags & GEX_EMIT_F_FOG_PASS) == 0u);
            CHECK((fog_v->flags & GEX_EMIT_F_FOG_PASS) != 0u);
            CHECK(main_v->color == tris[0].color[i]);
            CHECK(fog_v->color == tris[1].color[i]);
        }
    }
    gex_geometry_free(&g);
}

/* ------------------------------------------------------------------ */
/* Subdivisión real                                                   */
/* ------------------------------------------------------------------ */

static void test_subdivided(void)
{
    gex_emit_matrix m;
    gex_emit_output out;
    gex_geometry g;
    uint32_t color[3] = {0xFD010203u, 0x11223344u, 0x55667788u};
    uint8_t u[3] = {10, 20, 30};
    uint8_t v[3] = {40, 50, 60};
    int16_t vtx[9] = {-600, -600, 100, 600, 600, 100, 0, -600, 100};

    memset(&m, 0, sizeof(m));
    m.m[0] = 0x1000; m.m[4] = 0x1000; m.m[8] = 0x1000;
    m.focal = 0x140; m.cx = 0x100; m.cy = 0; m.near_z = 0x50;

    memset(&out, 0, sizeof(out));
    CHECK(gex_emit_subdivide(&m, vtx, color, u, v, 0x0123, 2, 0, 0, 999,
                             &out) == GEX_EMIT_OK);
    CHECK(out.count == 5u); /* 4 principales + 1 sliver */

    CHECK(gex_geometry_build(&out, &g) == GEX_GEOM_OK);
    CHECK(g.triangle_count == out.count);
    CHECK(g.index_count == out.count * 3u);
    if (g.vertices != NULL) {
        for (size_t i = 0; i < g.vertex_count; i++) {
            CHECK((g.vertices[i].flags & GEX_EMIT_F_SUBDIVIDED) != 0u);
            CHECK((g.vertices[i].flags & GEX_EMIT_F_NEAR_EMITTER) != 0u);
            CHECK((g.vertices[i].flags & GEX_EMIT_F_FOG_PASS) == 0u);
            CHECK(g.vertices[i].texture == 0x0123u);
            CHECK(g.vertices[i].layer == 2);
        }
    }
    gex_geometry_free(&g);
    gex_emit_output_free(&out);
}

/* ------------------------------------------------------------------ */
/* Extremo a extremo con una imagen DFX sintética                     */
/* ------------------------------------------------------------------ */

/* Mismo layout que test_level_emit.c, con z parametrizado:
 *   0x000 descriptor (64 B)   0x040 vértices (3 x 12 B)
 *   0x070 polígono (20 B)     0x090 registro UV (12 B)
 *   0x0A0 raíz BSP (32 B)     0x0C0 hoja 1 (32 B)
 *   0x0E0 hoja 2 vacía (32 B) */
static uint8_t *make_mini_image(size_t *out_sz, int16_t z0, int16_t z1,
                                int16_t z2)
{
    size_t sz = 0x0100;
    uint8_t *img = calloc(sz, 1);

    if (img == NULL) {
        return NULL;
    }
    *(uint32_t *)(img + 0x00) = 0xA0;
    *(uint32_t *)(img + 0x14) = 3;
    *(uint32_t *)(img + 0x18) = 1;
    *(uint32_t *)(img + 0x20) = 0x40;
    *(uint32_t *)(img + 0x24) = 0x70;

    uint8_t *vb = img + 0x40;
    *(int16_t *)(vb + 0) = 0;   *(int16_t *)(vb + 2) = 0;
    *(int16_t *)(vb + 4) = z0;  *(uint32_t *)(vb + 8) = 0xFF010203u;
    *(int16_t *)(vb + 12) = 0;  *(int16_t *)(vb + 14) = 100;
    *(int16_t *)(vb + 16) = z1; *(uint32_t *)(vb + 20) = 0xFF040506u;
    *(int16_t *)(vb + 24) = 100; *(int16_t *)(vb + 26) = 0;
    *(int16_t *)(vb + 28) = z2; *(uint32_t *)(vb + 32) = 0xFF070809u;

    uint16_t *pt = (uint16_t *)(img + 0x70);
    pt[0] = 0; pt[1] = 1; pt[2] = 2; pt[3] = 0;
    *(uint32_t *)(img + 0x70 + 0x10) = 0x90;

    uint8_t *uv = img + 0x90;
    /* El indice de textura esta en la mitad alta del SEGUNDO dword del
     * registro UV (+6), igual que en el original (0x46558A). */
    uv[0] = 1;  uv[1] = 2;  uv[2] = 0x34; uv[3] = 0x12;
    uv[4] = 3;  uv[5] = 4;  uv[6] = 0x23; uv[7] = 0x01;
    uv[8] = 5;  uv[9] = 6;  uv[10] = 0;   uv[11] = 7;

    *(uint16_t *)(img + 0xA0 + 0x12) = 1;
    *(uint32_t *)(img + 0xA0 + 0x18) = 0xC0;
    *(uint32_t *)(img + 0xA0 + 0x1C) = 0xE0;
    *(uint16_t *)(img + 0xC0 + 0x12) = 2;
    *(uint16_t *)(img + 0xC0 + 0x10) = 1;
    *(uint32_t *)(img + 0xC0 + 0x0C) = 0x70;
    *(uint16_t *)(img + 0xE0 + 0x12) = 2;
    *(uint16_t *)(img + 0xE0 + 0x10) = 0;
    *out_sz = sz;
    return img;
}

static void fill_desc(gex_dfx_descriptor *d)
{
    memset(d, 0, sizeof(*d));
    d->root_bsp = 0xA0;
    d->vertex_count = 3;
    d->polygon_count = 1;
    d->vertex_table = 0x40;
    d->polygon_table = 0x70;
}

static int visible_all(const uint8_t *image, size_t image_size,
                       uint32_t node_offset, void *ctx)
{
    (void)image; (void)image_size; (void)node_offset; (void)ctx;
    return 1;
}

static gex_emit_matrix identity_camera(int32_t cy)
{
    gex_emit_matrix m;
    memset(&m, 0, sizeof(m));
    m.m[0] = 0x1000; m.m[4] = 0x1000; m.m[8] = 0x1000;
    m.focal = 0x140; m.cx = 0x100; m.cy = cy; m.near_z = 0xA0;
    return m;
}

/* Reproduce el generador 0x461895 (igual que test_level_emit.c). */
static gex_emit_fog fog_with_lut(int16_t total_z, int16_t none_z)
{
    gex_emit_fog f;
    int32_t x = total_z, y = none_z;
    int32_t den = x - y;
    int32_t B = (x << 12) / den;
    int32_t A = -((x * y) / den);
    int32_t lim = 32767;
    int32_t ash;

    memset(&f, 0, sizeof(f));
    if (A > lim) A = lim;
    if (A < -lim) A = -lim;
    f.lut[0] = 0;
    for (uint32_t i = 1; i < 0x3620u; i += 4) {
        ash = (int32_t)((uint32_t)A << 12);
        int32_t val = B + (ash / (int32_t)i);
        if (val > 0x1000) val = 0x1000;
        if (val < 0) val = 0;
        f.lut[i >> 2] = (int16_t)val;
    }
    ash = (int32_t)((uint32_t)A << 12);
    f.have_lut = 1;
    f.fog_total_z = total_z;
    f.fog_none_z = none_z;
    f.threshold = (-ash) / B;
    f.split_z = f.threshold;
    f.fog_rgb[0] = 40; f.fog_rgb[1] = 60; f.fog_rgb[2] = 90;
    return f;
}

static void test_end_to_end_far(void)
{
    size_t sz = 0;
    uint8_t *img = make_mini_image(&sz, 2000, 2000, 2000);
    gex_dfx_descriptor desc;
    gex_bsp_walk_result r;
    gex_emit_matrix m;
    gex_emit_fog f;
    gex_emit_output out;
    gex_geometry g;

    if (img == NULL) {
        printf("skip end-to-end (sin memoria)\n");
        return;
    }
    fill_desc(&desc);
    memset(&r, 0, sizeof(r));
    CHECK(gex_bsp_walk(img, sz, 0xA0, visible_all, NULL, NULL, &r) == GEX_BSP_OK);
    CHECK(r.count == 2u);

    m = identity_camera(0xC0);
    f = fog_with_lut(400, 100);
    memset(&out, 0, sizeof(out));
    CHECK(gex_level_emit(img, sz, &desc, &r, &m, &f, 0, &out) == GEX_EMIT_OK);
    /* z = 2000 >= split_z y >= 0x6A4: emisor lejano -> principal + niebla. */
    CHECK(out.count == 2u);
    CHECK(out.fog_pass_count == 1u);

    CHECK(gex_geometry_build(&out, &g) == GEX_GEOM_OK);
    CHECK(g.triangle_count == 2u);
    CHECK(g.index_count == 6u);
    CHECK(g.vertex_count == 6u); /* principal y niebla no se fusionan */
    if (g.vertices != NULL) {
        int fog_seen = 0, main_seen = 0;
        for (size_t i = 0; i < g.vertex_count; i++) {
            if (g.vertices[i].flags & GEX_EMIT_F_FOG_PASS) {
                fog_seen++;
                CHECK((g.vertices[i].flags & GEX_EMIT_F_FAR_EMITTER) != 0u);
                CHECK((g.vertices[i].color >> 24) == 0u); /* alfa 0x00 */
            } else {
                main_seen++;
                CHECK((g.vertices[i].flags & GEX_EMIT_F_FAR_EMITTER) != 0u);
                CHECK((g.vertices[i].color >> 24) == 0xFFu);
            }
            CHECK(g.vertices[i].texture == 0x0123u);
            CHECK(g.vertices[i].layer == 7);
        }
        CHECK(fog_seen == 3 && main_seen == 3);
    }
    gex_geometry_free(&g);
    gex_emit_output_free(&out);
    gex_bsp_walk_free(&r);
    free(img);
}

static void test_end_to_end_subdivide(void)
{
    size_t sz = 0;
    uint8_t *img = make_mini_image(&sz, 400, 400, 300);
    gex_dfx_descriptor desc;
    gex_bsp_walk_result r;
    gex_emit_matrix m;
    gex_emit_fog f;
    gex_emit_output out;
    gex_geometry g;

    if (img == NULL) {
        printf("skip end-to-end (sin memoria)\n");
        return;
    }
    fill_desc(&desc);
    memset(&r, 0, sizeof(r));
    CHECK(gex_bsp_walk(img, sz, 0xA0, visible_all, NULL, NULL, &r) == GEX_BSP_OK);

    m = identity_camera(0xC0);
    f = fog_with_lut(400, 100);
    memset(&out, 0, sizeof(out));
    CHECK(gex_level_emit(img, sz, &desc, &r, &m, &f, 0, &out) == GEX_EMIT_OK);
    /* maxz 400 < 0x6A4: subdivisión (4 principales, sin niebla). */
    CHECK(out.count == 4u);
    CHECK(out.fog_pass_count == 0u);

    CHECK(gex_geometry_build(&out, &g) == GEX_GEOM_OK);
    CHECK(g.triangle_count == 4u);
    CHECK(g.index_count == 12u);
    CHECK(g.vertex_count >= 4u && g.vertex_count <= 12u);
    if (g.vertices != NULL) {
        for (size_t i = 0; i < g.vertex_count; i++) {
            CHECK((g.vertices[i].flags & GEX_EMIT_F_SUBDIVIDED) != 0u);
            CHECK((g.vertices[i].flags & GEX_EMIT_F_FOG_PASS) == 0u);
        }
    }
    gex_geometry_free(&g);
    gex_emit_output_free(&out);
    gex_bsp_walk_free(&r);
    free(img);
}

/* ------------------------------------------------------------------ */
/* Entrada vacía y argumentos nulos                                   */
/* ------------------------------------------------------------------ */

static void test_empty_and_null(void)
{
    gex_emit_output empty;
    gex_geometry g;
    gex_emit_triangle tri = mk_tri();
    gex_emit_output in = wrap(&tri, 1);

    memset(&empty, 0, sizeof(empty));
    memset(&g, 0xAA, sizeof(g));
    CHECK(gex_geometry_build(&empty, &g) == GEX_GEOM_OK);
    CHECK(g.vertices == NULL && g.indices == NULL);
    CHECK(g.vertex_count == 0u && g.index_count == 0u);
    CHECK(g.triangle_count == 0u && g.skipped_clipped == 0u);
    gex_geometry_free(&g);

    CHECK(gex_geometry_build(NULL, &g) == GEX_GEOM_ERR_ARG);
    CHECK(gex_geometry_build(&in, NULL) == GEX_GEOM_ERR_ARG);

    /* count > 0 con tris nulo. */
    in.tris = NULL;
    memset(&g, 0xAA, sizeof(g));
    CHECK(gex_geometry_build(&in, &g) == GEX_GEOM_ERR_ARG);
    CHECK(g.vertices == NULL && g.indices == NULL);
    CHECK(g.vertex_count == 0u);
    gex_geometry_free(&g);

    /* Nombres de estado. */
    CHECK(strcmp(gex_geom_status_name(GEX_GEOM_OK), "ok") == 0);
    CHECK(gex_geom_status_name(GEX_GEOM_ERR_NOMEM) != NULL);
    CHECK(gex_geom_status_name((gex_geom_status)999) != NULL);
}

/* ------------------------------------------------------------------ */
/* Límites y overflow                                                 */
/* ------------------------------------------------------------------ */

static void test_limits(void)
{
    gex_emit_triangle tri = mk_tri();
    gex_emit_output in = wrap(&tri, 1);
    gex_geometry g;

    /* count > capacity: incoherente. */
    in.capacity = 0;
    memset(&g, 0xAA, sizeof(g));
    CHECK(gex_geometry_build(&in, &g) == GEX_GEOM_ERR_RANGE);
    CHECK(g.vertices == NULL && g.vertex_count == 0u);

    /* count * 3 no representable: se rechaza antes de reservar. */
    in = wrap(&tri, SIZE_MAX / 2u);
    in.capacity = SIZE_MAX / 2u;
    CHECK(gex_geometry_build(&in, &g) == GEX_GEOM_ERR_OVERFLOW);
    CHECK(g.vertices == NULL && g.indices == NULL);

    in = wrap(&tri, SIZE_MAX / 3u + 1u);
    in.capacity = in.count;
    CHECK(gex_geometry_build(&in, &g) == GEX_GEOM_ERR_OVERFLOW);

    /* Justo por debajo del límite de uint32 sigue siendo rechazado si el
     * tamaño de la tabla no cabe: al menos no desborda ni reserva. */
    in = wrap(&tri, (size_t)UINT32_MAX / 3u + 1u);
    in.capacity = in.count;
    CHECK(gex_geometry_build(&in, &g) == GEX_GEOM_ERR_OVERFLOW);
    gex_geometry_free(&g);
}

/* ------------------------------------------------------------------ */
/* Fallo de reserva inyectado                                         */
/* ------------------------------------------------------------------ */

#ifdef GEX_GEOM_TEST_FAULT_INJECT
extern int gex_geom_test_fail_alloc;

static void test_alloc_failure(void)
{
    gex_emit_triangle tris[2];
    gex_emit_output in;
    gex_geometry g;

    tris[0] = mk_tri();
    tris[1] = mk_tri();
    tris[1].x[0] += 5;
    in = wrap(tris, 2);

    for (int n = 1; n <= 3; n++) {
        gex_geom_test_fail_alloc = n;
        memset(&g, 0xAA, sizeof(g));
        CHECK(gex_geometry_build(&in, &g) == GEX_GEOM_ERR_NOMEM);
        CHECK(g.vertices == NULL && g.indices == NULL);
        CHECK(g.vertex_count == 0u && g.index_count == 0u);
        gex_geometry_free(&g); /* no-op seguro tras el fallo */
    }
    gex_geom_test_fail_alloc = 0;
    CHECK(gex_geometry_build(&in, &g) == GEX_GEOM_OK);
    /* tris[1] solo cambia su primera esquina: 3 + 1 = 4 vértices. */
    CHECK(g.vertex_count == 4u);
    gex_geometry_free(&g);
}
#endif

/* ------------------------------------------------------------------ */
/* Liberación repetida y estados a cero                               */
/* ------------------------------------------------------------------ */

static void test_free_idempotent(void)
{
    gex_emit_triangle tri = mk_tri();
    gex_emit_output in = wrap(&tri, 1);
    gex_geometry g;

    CHECK(gex_geometry_build(&in, &g) == GEX_GEOM_OK);
    gex_geometry_free(&g);
    CHECK(g.vertices == NULL && g.indices == NULL);
    CHECK(g.vertex_count == 0u && g.index_count == 0u);
    gex_geometry_free(&g); /* segunda vez: no-op */
    gex_geometry_free(NULL);

    memset(&g, 0, sizeof(g));
    gex_geometry_free(&g); /* estructura ya a cero */
    CHECK(g.vertices == NULL);
}

/* ------------------------------------------------------------------ */
/* AZTEC16 real (solo lectura)                                        */
/* ------------------------------------------------------------------ */

static void test_aztec_real(void)
{
    FILE *fh = fopen(AZTEC_PATH, "rb");
    gex_dfx_container c;
    gex_dfx_level lv;
    gex_bsp_walk_result r;
    gex_emit_matrix m;
    gex_emit_fog f;
    gex_emit_output out;
    gex_geometry g;

    if (fh == NULL) {
        printf("skip aztec (no file)\n");
        return;
    }
    fclose(fh);

    memset(&c, 0, sizeof(c));
    CHECK(gex_dfx_load(AZTEC_PATH, &c) == GEX_DFX_OK);
    memset(&lv, 0, sizeof(lv));
    CHECK(gex_dfx_load_level(&c, &lv) == GEX_DFX_OK);
    CHECK(lv.descriptor.polygon_count == 9609u);

    memset(&r, 0, sizeof(r));
    CHECK(gex_bsp_walk(c.image, c.image_size, lv.descriptor.root_bsp,
                       visible_all, NULL, NULL, &r) == GEX_BSP_OK);
    CHECK(r.count == 815u);

    /* Misma cámara sintética que test_level_emit.c: profundidad en rango. */
    m = identity_camera(0xC0);
    m.m[0] = 0x0400;
    m.m[4] = 0x0400;
    m.m[8] = 0x0400;
    m.t[2] = 0x1000;
    f = fog_with_lut(400, 100);
    /* split_z = 0 fuerza el emisor lejano: registro principal + niebla. */
    f.split_z = 0;

    memset(&out, 0, sizeof(out));
    CHECK(gex_level_emit(c.image, c.image_size, &lv.descriptor, &r, &m, &f,
                         0, &out) == GEX_EMIT_OK);
    CHECK(out.count > 0u);
    CHECK(out.fog_pass_count > 0u);

    CHECK(gex_geometry_build(&out, &g) == GEX_GEOM_OK);
    CHECK(g.triangle_count == out.count);
    CHECK(g.index_count == out.count * 3u);
    CHECK(g.vertex_count > 0u && g.vertex_count <= out.count * 3u);
    /* La deduplicación debe reducir de verdad en un nivel real. */
    CHECK(g.vertex_count < out.count * 3u);
    if (g.indices != NULL) {
        for (size_t i = 0; i < g.index_count; i++) {
            CHECK(g.indices[i] < g.vertex_count);
        }
    }
    /* Determinismo: una segunda conversión da el mismo resultado. */
    {
        gex_geometry g2;
        CHECK(gex_geometry_build(&out, &g2) == GEX_GEOM_OK);
        CHECK(g2.vertex_count == g.vertex_count);
        CHECK(g2.index_count == g.index_count);
        CHECK(memcmp(g2.indices, g.indices,
                     g.index_count * sizeof(uint32_t)) == 0);
        gex_geometry_free(&g2);
    }

    printf("AZTEC16: triangles=%zu fog=%zu vertices=%zu indices=%zu\n",
           g.triangle_count, out.fog_pass_count, g.vertex_count,
           g.index_count);

    gex_geometry_free(&g);
    gex_emit_output_free(&out);
    gex_bsp_walk_free(&r);
    gex_dfx_release_level(&lv);
    gex_dfx_release_container(&c);
}

int main(void)
{
    test_single_triangle();
    test_multiple_triangles();
    test_clipped_skipped();
    test_fog_pass();
    test_subdivided();
    test_end_to_end_far();
    test_end_to_end_subdivide();
    test_empty_and_null();
    test_limits();
#ifdef GEX_GEOM_TEST_FAULT_INJECT
    test_alloc_failure();
#endif
    test_free_idempotent();
    test_aztec_real();

    printf("test_geometry: %d checks, %d failures\n", checks, failures);
    return failures == 0 ? 0 : 1;
}
