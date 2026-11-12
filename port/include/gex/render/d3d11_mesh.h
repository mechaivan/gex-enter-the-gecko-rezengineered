/*
 * gex/render/d3d11_mesh.h - Vertex/index buffers de Direct3D 11 para la
 * geometría del Módulo 3 (Windows-only).
 *
 * Consume la geometría portable de `gex/geometry.h` y crea un
 * ID3D11Buffer de vértices y otro de índices. NO crea shaders, NO crea el
 * D3D11_INPUT_ELEMENT_DESC (necesita la firma de un shader: Módulo 5), NO
 * carga texturas y NO dibuja. Es el "Hito 3" de
 * docs/D3D11_FIRST_SCENE_PLAN.md §13: recursos creados y verificables, sin
 * presentación.
 *
 * Windows-only como d3d11_device.h: el código portable no incluye esta
 * cabecera y el Makefile POSIX no compila la fuente.
 */

#ifndef GEX_RENDER_D3D11_MESH_H
#define GEX_RENDER_D3D11_MESH_H

#if defined(_WIN32)

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
/* WIDL C bindings: los ayudantes ID3D11Buffer_Release necesitan esto. */
#ifndef COBJMACROS
#define COBJMACROS
#endif
#include <windows.h>

#include <d3d11.h>

#include <stdint.h>

#include "gex/geometry.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef enum gex_d3d11_mesh_status {
    GEX_D3D11_MESH_OK = 0,
    GEX_D3D11_MESH_BAD_ARGUMENT,   /* device/geometry/mesh nulos */
    GEX_D3D11_MESH_BAD_STATE,      /* mesh ya inicializado */
    GEX_D3D11_MESH_BAD_GEOMETRY,   /* vacía, conteos o índices inválidos */
    GEX_D3D11_MESH_BUFFER_FAILED,  /* CreateBuffer falló; ver last_hresult */
    GEX_D3D11_MESH_OUT_OF_MEMORY   /* conversión de índices a 16 bits */
} gex_d3d11_mesh_status;

/*
 * Recursos de un dibujo. `vertex_stride` es siempre GEX_VERTEX_STRIDE (28).
 * `index_format` es DXGI_FORMAT_R16_UINT cuando vertex_count <= 65536 y
 * DXGI_FORMAT_R32_UINT en caso contrario; `index_stride` es 2 o 4.
 */
typedef struct gex_d3d11_mesh {
    ID3D11Buffer *vertex_buffer;
    ID3D11Buffer *index_buffer;
    UINT          vertex_count;
    UINT          index_count;
    UINT          vertex_stride;
    UINT          index_stride;
    DXGI_FORMAT   index_format;
    UINT          vertex_buffer_bytes;
    UINT          index_buffer_bytes;
    HRESULT       last_hresult;  /* S_OK o el fallo de CreateBuffer */
    int           initialized;   /* 1 = live; el único campo que lee create */
} gex_d3d11_mesh;

/*
 * PRE-CONDICIÓN: la estructura debe estar inicializada con
 * GEX_D3D11_MESH_INITIALIZER (o a cero) antes de la primera creación y antes
 * de destroy. destroy deja la estructura exactamente en ese estado, así que
 * se puede reutilizar y una segunda llamada es un no-op.
 */
#define GEX_D3D11_MESH_INITIALIZER                                            \
    {                                                                         \
        .vertex_buffer = NULL, .index_buffer = NULL, .vertex_count = 0,        \
        .index_count = 0, .vertex_stride = 0, .index_stride = 0,               \
        .index_format = DXGI_FORMAT_UNKNOWN, .vertex_buffer_bytes = 0,         \
        .index_buffer_bytes = 0, .last_hresult = S_OK, .initialized = 0        \
    }

/*
 * Crea los dos buffers a partir de geometría ya convertida.
 *
 *   mesh == NULL / device == NULL / geom == NULL  -> BAD_ARGUMENT
 *   mesh->initialized != 0                        -> BAD_STATE (no se toca)
 *   vertex_count == 0 o index_count == 0          -> BAD_GEOMETRY
 *   index_count % 3 != 0                          -> BAD_GEOMETRY
 *   algún índice >= vertex_count                  -> BAD_GEOMETRY
 *   tamaño en bytes > UINT_MAX                    -> BAD_GEOMETRY
 *   CreateBuffer falla                            -> BUFFER_FAILED
 *
 * En éxito la estructura es live y ambos buffers son propiedad del llamador
 * hasta `gex_d3d11_mesh_destroy`. En fallo no queda ningún recurso COM vivo y
 * `last_hresult` lleva el HRESULT (S_OK en los fallos de validación).
 */
gex_d3d11_mesh_status gex_d3d11_mesh_create(ID3D11Device *device,
                                            const gex_geometry *geom,
                                            gex_d3d11_mesh *mesh);

/*
 * Atajo: convierte `emit` con `gex_geometry_build` y crea los buffers, todo
 * en una llamada. La geometría intermedia se libera siempre. Errores de la
 * conversión se devuelven como BAD_GEOMETRY (argumentos/conteos) o
 * OUT_OF_MEMORY (reserva), con `last_hresult = S_OK`.
 */
gex_d3d11_mesh_status gex_d3d11_mesh_create_from_emit(ID3D11Device *device,
                                                      const gex_emit_output *emit,
                                                      gex_d3d11_mesh *mesh);

/*
 * Sustituye el contenido de los buffers por el de una geometría nueva.
 *
 * Es lo que necesita la secuencia de cámara del menú: la cámara se mueve en
 * cada update lógico, el BSP se vuelve a recorrer y la emisión cambia, así
 * que los buffers de vértices e índices tienen que reflejar la imagen nueva.
 *
 * Los buffers se **vuelven a crear** (el juego no reutiliza el contenido de
 * un buffer inmutable): el puntero al buffer cambia y el renderer, que
 * guarda el puntero a la malla, toma el nuevo en el siguiente dibujo. Los
 * conteos, el formato de índice y los tamaños se actualizan igual que en
 * `gex_d3d11_mesh_create`.
 *
 *   mesh == NULL / device == NULL / geom == NULL -> BAD_ARGUMENT
 *   mesh->initialized == 0                        -> BAD_STATE (no se toca)
 *   geometría inválida (mismos casos que create)  -> BAD_GEOMETRY
 *   CreateBuffer falla                            -> BUFFER_FAILED
 *
 * Si la creación del buffer de vértices falla, el de índices se libera y la
 * malla queda sin inicializar (nada vivo a medias).
 */
gex_d3d11_mesh_status gex_d3d11_mesh_update(ID3D11Device *device,
                                            const gex_geometry *geom,
                                            gex_d3d11_mesh *mesh);

/* Libera ambos buffers y deja la estructura a cero. Idempotente. */
void gex_d3d11_mesh_destroy(gex_d3d11_mesh *mesh);

const char *gex_d3d11_mesh_status_name(gex_d3d11_mesh_status status);

#ifdef __cplusplus
}
#endif

#endif /* _WIN32 */

#endif /* GEX_RENDER_D3D11_MESH_H */
