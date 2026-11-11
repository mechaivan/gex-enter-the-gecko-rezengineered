/*
 * test_d3d11_mesh.c - Módulo 3, fase D3D11: creación de vertex/index buffers
 * (Windows-only).
 *
 * Qué verifica (nunca dibuja: no hay shaders, ni swapchain, ni ventana):
 *   - nombres de estado y contrato del inicializador;
 *   - creación con el dispositivo WARP real de Windows: conteos, stride,
 *     formato de índice, ByteWidth y BindFlags de cada buffer;
 *   - lectura de vuelta (copia a un staging buffer) del contenido exacto de
 *     vértices e índices: los datos que llegan a la GPU son los convertidos;
 *   - elección de formato: R16_UINT hasta 65536 vértices, R32_UINT por encima;
 *   - errores: punteros nulos, geometría vacía, conteos incoherentes, índice
 *     fuera de rango, doble creación (BAD_STATE) y destrucción repetida;
 *   - create_from_emit de extremo a extremo;
 *   - hardware real cuando el runtime lo permite; si no, se imprime el
 *     límite y se omite el bloque (nunca un aprobado falso).
 */

#include "gex/render/d3d11_mesh.h"

#include "gex/render/d3d11_device.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

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
/* Helpers                                                            */
/* ------------------------------------------------------------------ */

static gex_emit_triangle mk_tri(void)
{
    gex_emit_triangle t;
    memset(&t, 0, sizeof(t));
    t.x[0] = 10; t.y[0] = 20; t.z[0] = 1000;
    t.x[1] = 30; t.y[1] = 20; t.z[1] = 1100;
    t.x[2] = 10; t.y[2] = 40; t.z[2] = 1200;
    t.color[0] = 0xFF112233u;
    t.color[1] = 0x80445566u;
    t.color[2] = 0x00778899u;
    t.u[0] = 0;   t.v[0] = 255;
    t.u[1] = 128; t.v[1] = 64;
    t.u[2] = 255; t.v[2] = 1;
    t.texture = 0x0123u;
    t.layer = -3;
    t.flags = GEX_EMIT_F_NEAR_EMITTER;
    return t;
}

static int mesh_is_initial(const gex_d3d11_mesh *m)
{
    return m->vertex_buffer == NULL && m->index_buffer == NULL &&
           m->vertex_count == 0u && m->index_count == 0u &&
           m->vertex_stride == 0u && m->index_stride == 0u &&
           m->index_format == DXGI_FORMAT_UNKNOWN &&
           m->vertex_buffer_bytes == 0u && m->index_buffer_bytes == 0u &&
           m->last_hresult == S_OK && m->initialized == 0;
}

/* Copia el contenido de un buffer inmutable a memoria del host. */
static int read_back(ID3D11Device *device, ID3D11DeviceContext *context,
                     ID3D11Buffer *src, void *dst, UINT bytes)
{
    D3D11_BUFFER_DESC desc;
    D3D11_MAPPED_SUBRESOURCE mapped;
    ID3D11Buffer *staging = NULL;
    HRESULT hr;

    ID3D11Buffer_GetDesc(src, &desc);
    desc.Usage = D3D11_USAGE_STAGING;
    desc.BindFlags = 0;
    desc.CPUAccessFlags = D3D11_CPU_ACCESS_READ;
    desc.MiscFlags = 0;

    hr = ID3D11Device_CreateBuffer(device, &desc, NULL, &staging);
    if (FAILED(hr) || staging == NULL) {
        return -1;
    }
    ID3D11DeviceContext_CopyResource(context, (ID3D11Resource *)staging,
                                     (ID3D11Resource *)src);
    hr = ID3D11DeviceContext_Map(context, (ID3D11Resource *)staging, 0,
                                 D3D11_MAP_READ, 0, &mapped);
    if (FAILED(hr)) {
        ID3D11Buffer_Release(staging);
        return -1;
    }
    memcpy(dst, mapped.pData, bytes);
    ID3D11DeviceContext_Unmap(context, (ID3D11Resource *)staging, 0);
    ID3D11Buffer_Release(staging);
    return 0;
}

/* ------------------------------------------------------------------ */
/* Nombres y contrato del inicializador                               */
/* ------------------------------------------------------------------ */

static void test_names(void)
{
    CHECK(strcmp(gex_d3d11_mesh_status_name(GEX_D3D11_MESH_OK), "ok") == 0);
    CHECK(gex_d3d11_mesh_status_name(GEX_D3D11_MESH_BAD_ARGUMENT) != NULL);
    CHECK(gex_d3d11_mesh_status_name(GEX_D3D11_MESH_BAD_STATE) != NULL);
    CHECK(gex_d3d11_mesh_status_name(GEX_D3D11_MESH_BAD_GEOMETRY) != NULL);
    CHECK(gex_d3d11_mesh_status_name(GEX_D3D11_MESH_BUFFER_FAILED) != NULL);
    CHECK(gex_d3d11_mesh_status_name(GEX_D3D11_MESH_OUT_OF_MEMORY) != NULL);
    CHECK(gex_d3d11_mesh_status_name((gex_d3d11_mesh_status)999) != NULL);
}

static void test_initializer(void)
{
    gex_d3d11_mesh a = GEX_D3D11_MESH_INITIALIZER;
    gex_d3d11_mesh b;

    memset(&b, 0, sizeof(b));
    CHECK(mesh_is_initial(&a));
    CHECK(memcmp(&a, &b, sizeof(a)) == 0);

    gex_d3d11_mesh_destroy(&a); /* ya inicial: no-op */
    CHECK(mesh_is_initial(&a));
    gex_d3d11_mesh_destroy(NULL);
}

/* ------------------------------------------------------------------ */
/* Creación real con WARP                                             */
/* ------------------------------------------------------------------ */

static void test_create_warp(void)
{
    gex_d3d11_device dev = GEX_D3D11_DEVICE_INITIALIZER;
    gex_d3d11_device_status ds;
    gex_emit_triangle tris[3];
    gex_emit_output emit;
    gex_geometry geom;
    gex_d3d11_mesh mesh = GEX_D3D11_MESH_INITIALIZER;
    gex_d3d11_mesh_status st;
    D3D11_BUFFER_DESC vdesc, idesc;
    gex_vertex *vread = NULL;
    uint16_t *iread = NULL;

    ds = gex_d3d11_device_init(&dev, GEX_D3D11_DRIVER_WARP, 0);
    if (ds != GEX_D3D11_DEVICE_OK) {
        printf("skip WARP: %s (0x%08lx)\n", gex_d3d11_device_status_name(ds),
               (unsigned long)dev.last_hresult);
        return;
    }

    tris[0] = mk_tri();
    tris[1] = mk_tri();
    tris[1].flags = GEX_EMIT_F_FOG_PASS | GEX_EMIT_F_FAR_EMITTER;
    tris[1].color[0] = 0x00102030u;
    tris[1].color[1] = 0x00102031u;
    tris[1].color[2] = 0x00102032u;
    tris[2] = mk_tri();
    tris[2].x[0] += 200;
    tris[2].x[1] += 200;
    tris[2].x[2] += 200;
    memset(&emit, 0, sizeof(emit));
    emit.tris = tris;
    emit.count = 3;
    emit.capacity = 3;

    CHECK(gex_geometry_build(&emit, &geom) == GEX_GEOM_OK);
    CHECK(geom.vertex_count == 9u);
    CHECK(geom.index_count == 9u);

    st = gex_d3d11_mesh_create(dev.device, &geom, &mesh);
    CHECK(st == GEX_D3D11_MESH_OK);
    CHECK(mesh.initialized == 1);
    CHECK(mesh.last_hresult == S_OK);
    CHECK(mesh.vertex_count == 9u);
    CHECK(mesh.index_count == 9u);
    CHECK(mesh.vertex_stride == GEX_VERTEX_STRIDE);
    CHECK(mesh.index_stride == 2u);
    CHECK(mesh.index_format == DXGI_FORMAT_R16_UINT);
    CHECK(mesh.vertex_buffer_bytes == 9u * GEX_VERTEX_STRIDE);
    CHECK(mesh.index_buffer_bytes == 9u * 2u);
    CHECK(mesh.vertex_buffer != NULL && mesh.index_buffer != NULL);

    if (mesh.vertex_buffer != NULL) {
        ID3D11Buffer_GetDesc(mesh.vertex_buffer, &vdesc);
        CHECK(vdesc.ByteWidth == 9u * GEX_VERTEX_STRIDE);
        CHECK(vdesc.Usage == D3D11_USAGE_IMMUTABLE);
        CHECK((vdesc.BindFlags & D3D11_BIND_VERTEX_BUFFER) != 0u);
        CHECK(vdesc.CPUAccessFlags == 0u);
    }
    if (mesh.index_buffer != NULL) {
        ID3D11Buffer_GetDesc(mesh.index_buffer, &idesc);
        CHECK(idesc.ByteWidth == 9u * 2u);
        CHECK(idesc.Usage == D3D11_USAGE_IMMUTABLE);
        CHECK((idesc.BindFlags & D3D11_BIND_INDEX_BUFFER) != 0u);
        CHECK(idesc.CPUAccessFlags == 0u);
    }

    /* Contenido exacto: lo que se creó es lo que se convirtió. */
    vread = (gex_vertex *)malloc(9u * sizeof(gex_vertex));
    iread = (uint16_t *)malloc(9u * sizeof(uint16_t));
    if (vread != NULL && iread != NULL) {
        CHECK(read_back(dev.device, dev.context, mesh.vertex_buffer, vread,
                        9u * GEX_VERTEX_STRIDE) == 0);
        CHECK(memcmp(vread, geom.vertices, 9u * sizeof(gex_vertex)) == 0);
        CHECK(read_back(dev.device, dev.context, mesh.index_buffer, iread,
                        9u * sizeof(uint16_t)) == 0);
        for (int i = 0; i < 9; i++) {
            CHECK(iread[i] == (uint16_t)geom.indices[i]);
        }
        /* Atributos conservados en la GPU: color, UV, textura, capa, flags. */
        CHECK(vread[0].color == tris[0].color[0]);
        CHECK(vread[0].texture == 0x0123u);
        CHECK(vread[0].layer == -3);
        CHECK(vread[0].flags == GEX_EMIT_F_NEAR_EMITTER);
        CHECK(vread[3].flags ==
              (GEX_EMIT_F_FOG_PASS | GEX_EMIT_F_FAR_EMITTER));
        CHECK(vread[3].color == 0x00102030u);
        CHECK(vread[6].x == (float)(tris[2].x[0]));
    } else {
        printf("skip read-back (sin memoria del host)\n");
    }

    /* Doble creación: BAD_STATE y el mesh vivo queda intacto. */
    {
        gex_d3d11_mesh before = mesh;
        CHECK(gex_d3d11_mesh_create(dev.device, &geom, &mesh) ==
              GEX_D3D11_MESH_BAD_STATE);
        CHECK(memcmp(&before, &mesh, sizeof(mesh)) == 0);
    }

    gex_d3d11_mesh_destroy(&mesh);
    CHECK(mesh_is_initial(&mesh));
    gex_d3d11_mesh_destroy(&mesh); /* segunda vez: no-op */
    CHECK(mesh_is_initial(&mesh));

    free(vread);
    free(iread);
    gex_geometry_free(&geom);
    gex_d3d11_device_shutdown(&dev);
}

/* ------------------------------------------------------------------ */
/* Formato de índice: frontera R16 / R32                              */
/* ------------------------------------------------------------------ */

static void test_index_formats(void)
{
    gex_d3d11_device dev = GEX_D3D11_DEVICE_INITIALIZER;
    gex_geometry geom;
    gex_d3d11_mesh mesh = GEX_D3D11_MESH_INITIALIZER;
    gex_d3d11_mesh_status st;

    if (gex_d3d11_device_init(&dev, GEX_D3D11_DRIVER_WARP, 0) !=
        GEX_D3D11_DEVICE_OK) {
        printf("skip index formats (sin WARP)\n");
        return;
    }

    /* 65536 vértices: el índice máximo 65535 sigue cabiendo en 16 bits. */
    memset(&geom, 0, sizeof(geom));
    geom.vertex_count = 0x10000u;
    geom.vertices = (gex_vertex *)calloc(geom.vertex_count, sizeof(gex_vertex));
    geom.indices = (uint32_t *)malloc(3u * sizeof(uint32_t));
    if (geom.vertices == NULL || geom.indices == NULL) {
        printf("skip index formats (sin memoria)\n");
        free(geom.vertices);
        free(geom.indices);
        gex_d3d11_device_shutdown(&dev);
        return;
    }
    geom.indices[0] = 0;
    geom.indices[1] = 0xFFFFu;
    geom.indices[2] = 1;
    geom.index_count = 3;
    geom.triangle_count = 1;

    st = gex_d3d11_mesh_create(dev.device, &geom, &mesh);
    CHECK(st == GEX_D3D11_MESH_OK);
    CHECK(mesh.index_stride == 2u);
    CHECK(mesh.index_format == DXGI_FORMAT_R16_UINT);
    if (mesh.index_buffer != NULL) {
        uint16_t idx16[3] = {0, 0, 0};
        CHECK(read_back(dev.device, dev.context, mesh.index_buffer, idx16,
                        3u * sizeof(uint16_t)) == 0);
        CHECK(idx16[0] == 0u && idx16[1] == 0xFFFFu && idx16[2] == 1u);
    }
    gex_d3d11_mesh_destroy(&mesh);
    gex_geometry_free(&geom);

    /* 65537 vértices: ya no cabe en 16 bits -> R32_UINT. */
    memset(&geom, 0, sizeof(geom));
    geom.vertex_count = 0x10001u;
    geom.vertices = (gex_vertex *)calloc(geom.vertex_count, sizeof(gex_vertex));
    geom.indices = (uint32_t *)malloc(3u * sizeof(uint32_t));
    if (geom.vertices == NULL || geom.indices == NULL) {
        printf("skip index formats (sin memoria)\n");
        free(geom.vertices);
        free(geom.indices);
        gex_d3d11_device_shutdown(&dev);
        return;
    }
    geom.indices[0] = 0;
    geom.indices[1] = 0x10000u;
    geom.indices[2] = 2;
    geom.index_count = 3;
    geom.triangle_count = 1;

    st = gex_d3d11_mesh_create(dev.device, &geom, &mesh);
    CHECK(st == GEX_D3D11_MESH_OK);
    CHECK(mesh.index_stride == 4u);
    CHECK(mesh.index_format == DXGI_FORMAT_R32_UINT);
    if (mesh.index_buffer != NULL) {
        uint32_t idx32[3] = {0, 0, 0};
        CHECK(read_back(dev.device, dev.context, mesh.index_buffer, idx32,
                        3u * sizeof(uint32_t)) == 0);
        CHECK(idx32[0] == 0u && idx32[1] == 0x10000u && idx32[2] == 2u);
    }
    gex_d3d11_mesh_destroy(&mesh);
    gex_geometry_free(&geom);

    gex_d3d11_device_shutdown(&dev);
}

/* ------------------------------------------------------------------ */
/* Errores y estados                                                  */
/* ------------------------------------------------------------------ */

static void test_errors(void)
{
    gex_d3d11_device dev = GEX_D3D11_DEVICE_INITIALIZER;
    gex_d3d11_mesh mesh = GEX_D3D11_MESH_INITIALIZER;
    gex_geometry geom;
    gex_emit_triangle tri = mk_tri();
    gex_emit_output emit;
    gex_vertex v[3];
    uint32_t idx[4];

    memset(&geom, 0, sizeof(geom));
    memset(&emit, 0, sizeof(emit));
    CHECK(gex_d3d11_mesh_create(NULL, &geom, &mesh) ==
          GEX_D3D11_MESH_BAD_ARGUMENT);
    CHECK(gex_d3d11_mesh_create(NULL, NULL, NULL) ==
          GEX_D3D11_MESH_BAD_ARGUMENT);
    CHECK(gex_d3d11_mesh_create_from_emit(NULL, NULL, NULL) ==
          GEX_D3D11_MESH_BAD_ARGUMENT);

    if (gex_d3d11_device_init(&dev, GEX_D3D11_DRIVER_WARP, 0) !=
        GEX_D3D11_DEVICE_OK) {
        printf("skip error checks (sin WARP)\n");
        return;
    }

    CHECK(gex_d3d11_mesh_create(dev.device, NULL, &mesh) ==
          GEX_D3D11_MESH_BAD_ARGUMENT);
    CHECK(gex_d3d11_mesh_create(dev.device, &geom, NULL) ==
          GEX_D3D11_MESH_BAD_ARGUMENT);

    /* Geometría vacía. */
    memset(&geom, 0, sizeof(geom));
    CHECK(gex_d3d11_mesh_create(dev.device, &geom, &mesh) ==
          GEX_D3D11_MESH_BAD_GEOMETRY);
    CHECK(mesh_is_initial(&mesh));

    /* Punteros nulos con conteos > 0. */
    geom.vertex_count = 3;
    geom.index_count = 3;
    CHECK(gex_d3d11_mesh_create(dev.device, &geom, &mesh) ==
          GEX_D3D11_MESH_BAD_GEOMETRY);

    /* index_count no múltiplo de 3. */
    memset(v, 0, sizeof(v));
    geom.vertices = v;
    geom.indices = idx;
    geom.index_count = 4;
    idx[0] = idx[1] = idx[2] = idx[3] = 0;
    CHECK(gex_d3d11_mesh_create(dev.device, &geom, &mesh) ==
          GEX_D3D11_MESH_BAD_GEOMETRY);

    /* Índice fuera de rango. */
    geom.index_count = 3;
    idx[0] = 0; idx[1] = 1; idx[2] = 3; /* 3 >= vertex_count */
    CHECK(gex_d3d11_mesh_create(dev.device, &geom, &mesh) ==
          GEX_D3D11_MESH_BAD_GEOMETRY);
    CHECK(mesh_is_initial(&mesh));

    /* create_from_emit: conteos incoherentes y lista vacía. */
    memset(&emit, 0, sizeof(emit));
    emit.tris = &tri;
    emit.count = 1;
    emit.capacity = 0;
    CHECK(gex_d3d11_mesh_create_from_emit(dev.device, &emit, &mesh) ==
          GEX_D3D11_MESH_BAD_GEOMETRY);

    memset(&emit, 0, sizeof(emit));
    CHECK(gex_d3d11_mesh_create_from_emit(dev.device, &emit, &mesh) ==
          GEX_D3D11_MESH_BAD_GEOMETRY);

    /* create_from_emit válido: 1 triángulo -> 3 vértices, 3 índices. */
    memset(&emit, 0, sizeof(emit));
    emit.tris = &tri;
    emit.count = 1;
    emit.capacity = 1;
    CHECK(gex_d3d11_mesh_create_from_emit(dev.device, &emit, &mesh) ==
          GEX_D3D11_MESH_OK);
    CHECK(mesh.vertex_count == 3u);
    CHECK(mesh.index_count == 3u);
    gex_d3d11_mesh_destroy(&mesh);
    CHECK(mesh_is_initial(&mesh));

    gex_d3d11_device_shutdown(&dev);
}

/* ------------------------------------------------------------------ */
/* Hardware, si el runtime lo permite                                 */
/* ------------------------------------------------------------------ */

static void test_hardware(void)
{
    gex_d3d11_device dev = GEX_D3D11_DEVICE_INITIALIZER;
    gex_d3d11_device_status ds;
    gex_emit_triangle tri = mk_tri();
    gex_emit_output emit;
    gex_d3d11_mesh mesh = GEX_D3D11_MESH_INITIALIZER;

    ds = gex_d3d11_device_init(&dev, GEX_D3D11_DRIVER_HARDWARE, 0);
    if (ds != GEX_D3D11_DEVICE_OK) {
        printf("skip hardware: %s (0x%08lx)\n",
               gex_d3d11_device_status_name(ds),
               (unsigned long)dev.last_hresult);
        return;
    }

    memset(&emit, 0, sizeof(emit));
    emit.tris = &tri;
    emit.count = 1;
    emit.capacity = 1;
    CHECK(gex_d3d11_mesh_create_from_emit(dev.device, &emit, &mesh) ==
          GEX_D3D11_MESH_OK);
    CHECK(mesh.vertex_count == 3u);
    CHECK(mesh.index_count == 3u);
    gex_d3d11_mesh_destroy(&mesh);
    CHECK(mesh_is_initial(&mesh));

    printf("hardware: %s, feature level %s\n",
           gex_d3d11_device_status_name(ds),
           gex_d3d11_feature_level_name(dev.feature_level));
    gex_d3d11_device_shutdown(&dev);
}

/* ------------------------------------------------------------------ */
/* AZTEC16 real: buffers con la geometría completa del nivel          */
/* ------------------------------------------------------------------ */

#define AZTEC_PATH "C:/GEX_REZ/E2_A_ORIGINAL/LEVEL/AZTEC16.DFX"

static int visible_all(const uint8_t *image, size_t image_size,
                       uint32_t node_offset, void *ctx)
{
    (void)image; (void)image_size; (void)node_offset; (void)ctx;
    return 1;
}

static gex_emit_fog fog_with_lut(int16_t total_z, int16_t none_z)
{
    /* Reproduce el generador 0x461895 (igual que test_geometry.c). */
    gex_emit_fog f;
    int32_t x = total_z, y = none_z;
    int32_t den = x - y;
    int32_t B = (x << 12) / den;
    int32_t A = -((x * y) / den);
    int32_t lim = 32767;

    memset(&f, 0, sizeof(f));
    if (A > lim) A = lim;
    if (A < -lim) A = -lim;
    f.lut[0] = 0;
    for (uint32_t i = 1; i < 0x3620u; i += 4) {
        int32_t ash = (int32_t)((uint32_t)A << 12);
        int32_t val = B + (ash / (int32_t)i);
        if (val > 0x1000) val = 0x1000;
        if (val < 0) val = 0;
        f.lut[i >> 2] = (int16_t)val;
    }
    f.have_lut = 1;
    f.fog_total_z = total_z;
    f.fog_none_z = none_z;
    f.threshold = 0;
    f.split_z = 0; /* fuerza el emisor lejano: principal + niebla */
    f.fog_rgb[0] = 40; f.fog_rgb[1] = 60; f.fog_rgb[2] = 90;
    return f;
}

static void test_aztec_real(void)
{
    gex_d3d11_device dev = GEX_D3D11_DEVICE_INITIALIZER;
    gex_dfx_container c;
    gex_dfx_level lv;
    gex_bsp_walk_result r;
    gex_emit_matrix m;
    gex_emit_fog f;
    gex_emit_output out;
    gex_geometry geom;
    gex_d3d11_mesh mesh = GEX_D3D11_MESH_INITIALIZER;
    D3D11_BUFFER_DESC vdesc, idesc;
    FILE *fh = fopen(AZTEC_PATH, "rb");

    if (fh == NULL) {
        printf("skip aztec (no file)\n");
        return;
    }
    fclose(fh);

    if (gex_d3d11_device_init(&dev, GEX_D3D11_DRIVER_WARP, 0) !=
        GEX_D3D11_DEVICE_OK) {
        printf("skip aztec (sin WARP)\n");
        return;
    }

    memset(&c, 0, sizeof(c));
    CHECK(gex_dfx_load(AZTEC_PATH, &c) == GEX_DFX_OK);
    memset(&lv, 0, sizeof(lv));
    CHECK(gex_dfx_load_level(&c, &lv) == GEX_DFX_OK);
    memset(&r, 0, sizeof(r));
    CHECK(gex_bsp_walk(c.image, c.image_size, lv.descriptor.root_bsp,
                       visible_all, NULL, NULL, &r) == GEX_BSP_OK);

    memset(&m, 0, sizeof(m));
    m.m[0] = 0x0400; m.m[4] = 0x0400; m.m[8] = 0x0400;
    m.focal = 0x140; m.cx = 0x100; m.cy = 0x0C0; m.near_z = 0xA0;
    m.t[2] = 0x1000;
    f = fog_with_lut(400, 100);

    memset(&out, 0, sizeof(out));
    CHECK(gex_level_emit(c.image, c.image_size, &lv.descriptor, &r, &m, &f,
                         0, &out) == GEX_EMIT_OK);
    CHECK(out.count > 0u);
    CHECK(out.fog_pass_count > 0u);

    CHECK(gex_geometry_build(&out, &geom) == GEX_GEOM_OK);
    CHECK(geom.triangle_count == out.count);
    CHECK(geom.index_count == out.count * 3u);

    CHECK(gex_d3d11_mesh_create(dev.device, &geom, &mesh) ==
          GEX_D3D11_MESH_OK);
    CHECK(mesh.initialized == 1);
    CHECK(mesh.vertex_count == (UINT)geom.vertex_count);
    CHECK(mesh.index_count == (UINT)geom.index_count);
    CHECK(mesh.index_stride == 2u);
    CHECK(mesh.index_format == DXGI_FORMAT_R16_UINT);
    if (mesh.vertex_buffer != NULL) {
        ID3D11Buffer_GetDesc(mesh.vertex_buffer, &vdesc);
        CHECK(vdesc.ByteWidth == (UINT)geom.vertex_count * GEX_VERTEX_STRIDE);
        CHECK((vdesc.BindFlags & D3D11_BIND_VERTEX_BUFFER) != 0u);
    }
    if (mesh.index_buffer != NULL) {
        ID3D11Buffer_GetDesc(mesh.index_buffer, &idesc);
        CHECK(idesc.ByteWidth == (UINT)geom.index_count * 2u);
        CHECK((idesc.BindFlags & D3D11_BIND_INDEX_BUFFER) != 0u);
    }

    /* Los índices leídos de la GPU siguen apuntando dentro del vertex buffer. */
    if (mesh.index_buffer != NULL && geom.index_count <= 16384u) {
        uint16_t *idx16 = (uint16_t *)malloc(geom.index_count * sizeof(uint16_t));
        if (idx16 != NULL) {
            CHECK(read_back(dev.device, dev.context, mesh.index_buffer, idx16,
                            (UINT)(geom.index_count * sizeof(uint16_t))) == 0);
            {
                size_t bad = 0;
                for (size_t i = 0; i < geom.index_count; i++) {
                    if ((size_t)idx16[i] >= geom.vertex_count) {
                        bad++;
                    }
                    if (idx16[i] != (uint16_t)geom.indices[i]) {
                        bad++;
                    }
                }
                CHECK(bad == 0u);
            }
            free(idx16);
        }
    }

    printf("aztec: triangles=%zu vertices=%zu indices=%zu buffers=%ux%u B\n",
           geom.triangle_count, geom.vertex_count, geom.index_count,
           mesh.vertex_buffer_bytes, mesh.index_buffer_bytes);

    gex_d3d11_mesh_destroy(&mesh);
    CHECK(mesh_is_initial(&mesh));
    gex_geometry_free(&geom);
    gex_emit_output_free(&out);
    gex_bsp_walk_free(&r);
    gex_dfx_release_level(&lv);
    gex_dfx_release_container(&c);
    gex_d3d11_device_shutdown(&dev);
}

int main(void)
{
    test_names();
    test_initializer();
    test_create_warp();
    test_index_formats();
    test_errors();
    test_hardware();
    test_aztec_real();

    printf("test_d3d11_mesh: %d checks, %d failures\n", checks, failures);
    return failures == 0 ? 0 : 1;
}
