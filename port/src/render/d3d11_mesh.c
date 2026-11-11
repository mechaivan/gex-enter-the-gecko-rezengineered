/*
 * d3d11_mesh.c - Vertex/index buffers de Direct3D 11 (Módulo 3,
 * Windows-only). Véase gex/render/d3d11_mesh.h y
 * docs/D3D11_FIRST_SCENE_PLAN.md §13.
 *
 * No crea shaders, ni input layout, ni texturas, ni dibuja: solo recursos.
 */

#include "gex/render/d3d11_mesh.h"

#if defined(_WIN32)

#include <stdlib.h>
#include <string.h>

gex_d3d11_mesh_status gex_d3d11_mesh_create(ID3D11Device *device,
                                            const gex_geometry *geom,
                                            gex_d3d11_mesh *mesh)
{
    D3D11_BUFFER_DESC desc;
    D3D11_SUBRESOURCE_DATA init;
    const void *index_data;
    UINT16 *indices16 = NULL;
    UINT index_stride;
    DXGI_FORMAT index_format;
    UINT vertex_bytes;
    UINT index_bytes;
    size_t i;
    HRESULT hr;

    if (device == NULL || geom == NULL || mesh == NULL) {
        return GEX_D3D11_MESH_BAD_ARGUMENT;
    }
    /* Un mesh vivo no se toca: el llamador debe destruirlo antes. */
    if (mesh->initialized != 0) {
        return GEX_D3D11_MESH_BAD_STATE;
    }
    memset(mesh, 0, sizeof(*mesh)); /* last_hresult = S_OK */
    mesh->last_hresult = S_OK;

    /* Validación de la geometría antes de reservar o crear nada. */
    if (geom->vertex_count == 0 || geom->index_count == 0 ||
        geom->vertices == NULL || geom->indices == NULL) {
        return GEX_D3D11_MESH_BAD_GEOMETRY;
    }
    if (geom->index_count % 3u != 0) {
        return GEX_D3D11_MESH_BAD_GEOMETRY;
    }
    if (geom->vertex_count > (size_t)UINT_MAX ||
        geom->index_count > (size_t)UINT_MAX) {
        return GEX_D3D11_MESH_BAD_GEOMETRY;
    }
    if (geom->vertex_count > SIZE_MAX / sizeof(gex_vertex)) {
        return GEX_D3D11_MESH_BAD_GEOMETRY;
    }
    if (geom->vertex_count * sizeof(gex_vertex) > (size_t)UINT_MAX) {
        return GEX_D3D11_MESH_BAD_GEOMETRY;
    }
    /* Todo índice debe apuntar dentro del vertex buffer. */
    for (i = 0; i < geom->index_count; i++) {
        if ((size_t)geom->indices[i] >= geom->vertex_count) {
            return GEX_D3D11_MESH_BAD_GEOMETRY;
        }
    }

    /* R16_UINT cubre hasta 65536 vértices (índices 0..65535). */
    if (geom->vertex_count <= 0x10000u) {
        index_stride = 2u;
        index_format = DXGI_FORMAT_R16_UINT;
    } else {
        index_stride = 4u;
        index_format = DXGI_FORMAT_R32_UINT;
    }
    if (geom->index_count > (size_t)UINT_MAX / index_stride) {
        return GEX_D3D11_MESH_BAD_GEOMETRY;
    }
    vertex_bytes = (UINT)(geom->vertex_count * sizeof(gex_vertex));
    index_bytes = (UINT)(geom->index_count * index_stride);

    if (index_stride == 2u) {
        indices16 = (UINT16 *)malloc((size_t)index_bytes);
        if (indices16 == NULL) {
            return GEX_D3D11_MESH_OUT_OF_MEMORY;
        }
        for (i = 0; i < geom->index_count; i++) {
            indices16[i] = (UINT16)geom->indices[i];
        }
        index_data = indices16;
    } else {
        index_data = geom->indices;
    }

    /* Vertex buffer: inmutable (la geometría del nivel no cambia por frame). */
    memset(&desc, 0, sizeof(desc));
    desc.ByteWidth = vertex_bytes;
    desc.Usage = D3D11_USAGE_IMMUTABLE;
    desc.BindFlags = D3D11_BIND_VERTEX_BUFFER;
    desc.CPUAccessFlags = 0;
    desc.MiscFlags = 0;
    desc.StructureByteStride = 0;

    memset(&init, 0, sizeof(init));
    init.pSysMem = geom->vertices;
    init.SysMemPitch = 0;
    init.SysMemSlicePitch = 0;

    hr = ID3D11Device_CreateBuffer(device, &desc, &init, &mesh->vertex_buffer);
    if (FAILED(hr)) {
        mesh->last_hresult = hr;
        mesh->vertex_buffer = NULL;
        free(indices16);
        return GEX_D3D11_MESH_BUFFER_FAILED;
    }

    memset(&desc, 0, sizeof(desc));
    desc.ByteWidth = index_bytes;
    desc.Usage = D3D11_USAGE_IMMUTABLE;
    desc.BindFlags = D3D11_BIND_INDEX_BUFFER;
    desc.CPUAccessFlags = 0;
    desc.MiscFlags = 0;
    desc.StructureByteStride = 0;

    memset(&init, 0, sizeof(init));
    init.pSysMem = index_data;
    init.SysMemPitch = 0;
    init.SysMemSlicePitch = 0;

    hr = ID3D11Device_CreateBuffer(device, &desc, &init, &mesh->index_buffer);
    free(indices16);
    if (FAILED(hr)) {
        mesh->last_hresult = hr;
        mesh->index_buffer = NULL;
        if (mesh->vertex_buffer != NULL) {
            ID3D11Buffer_Release(mesh->vertex_buffer);
            mesh->vertex_buffer = NULL;
        }
        return GEX_D3D11_MESH_BUFFER_FAILED;
    }

    mesh->vertex_count = (UINT)geom->vertex_count;
    mesh->index_count = (UINT)geom->index_count;
    mesh->vertex_stride = GEX_VERTEX_STRIDE;
    mesh->index_stride = index_stride;
    mesh->index_format = index_format;
    mesh->vertex_buffer_bytes = vertex_bytes;
    mesh->index_buffer_bytes = index_bytes;
    mesh->last_hresult = S_OK;
    mesh->initialized = 1;
    return GEX_D3D11_MESH_OK;
}

gex_d3d11_mesh_status gex_d3d11_mesh_create_from_emit(ID3D11Device *device,
                                                      const gex_emit_output *emit,
                                                      gex_d3d11_mesh *mesh)
{
    gex_geometry geom;
    gex_geom_status gst;
    gex_d3d11_mesh_status st;

    if (device == NULL || emit == NULL || mesh == NULL) {
        return GEX_D3D11_MESH_BAD_ARGUMENT;
    }
    if (mesh->initialized != 0) {
        return GEX_D3D11_MESH_BAD_STATE;
    }

    gst = gex_geometry_build(emit, &geom);
    if (gst != GEX_GEOM_OK) {
        memset(mesh, 0, sizeof(*mesh));
        mesh->last_hresult = S_OK;
        return (gst == GEX_GEOM_ERR_NOMEM) ? GEX_D3D11_MESH_OUT_OF_MEMORY
                                           : GEX_D3D11_MESH_BAD_GEOMETRY;
    }
    st = gex_d3d11_mesh_create(device, &geom, mesh);
    gex_geometry_free(&geom);
    return st;
}

void gex_d3d11_mesh_destroy(gex_d3d11_mesh *mesh)
{
    if (mesh == NULL) {
        return;
    }
    if (mesh->index_buffer != NULL) {
        ID3D11Buffer_Release(mesh->index_buffer);
    }
    if (mesh->vertex_buffer != NULL) {
        ID3D11Buffer_Release(mesh->vertex_buffer);
    }
    memset(mesh, 0, sizeof(*mesh));
}

const char *gex_d3d11_mesh_status_name(gex_d3d11_mesh_status status)
{
    switch (status) {
    case GEX_D3D11_MESH_OK:
        return "ok";
    case GEX_D3D11_MESH_BAD_ARGUMENT:
        return "bad argument";
    case GEX_D3D11_MESH_BAD_STATE:
        return "bad state";
    case GEX_D3D11_MESH_BAD_GEOMETRY:
        return "bad geometry";
    case GEX_D3D11_MESH_BUFFER_FAILED:
        return "buffer creation failed";
    case GEX_D3D11_MESH_OUT_OF_MEMORY:
        return "out of memory";
    }
    return "unknown";
}

#endif /* _WIN32 */
