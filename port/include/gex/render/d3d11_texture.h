/*
 * gex/render/d3d11_texture.h - recursos de textura de Direct3D 11 a partir
 * de las texturas decodificadas del `.VFX` (Módulo 4C, Windows-only).
 *
 * Estrategia de recursos (evaluada, no supuesta):
 *
 *  - El nivel usa **varias** texturas con **dimensiones distintas** y el
 *    índice de textura es **por polígono**, no por vértice. D3D11 solo
 *    permite un recurso de shader por llamada de dibujo, así que hace falta
 *    una de dos cosas: varias llamadas de dibujo (una por textura) o un
 *    recurso único del que el shader elige.
 *  - Varias llamadas de dibujo **romperían el orden del painter**: el
 *    port dibuja en el orden de emisión (el algoritmo del painter del
 *    original, sin depth buffer), así que reordenar las primitivas
 *    cambiaría el resultado visual. **Descartado.**
 *  - Una **texture array** (una rebanada por textura) es la opción más
 *    directa, pero se ha **descartado en la práctica**: en el equipo de
 *    desarrollo la creación de un array 2D con datos iniciales falla con
 *    E_INVALIDARG (hardware y WARP) y llega a colgar el driver. Es un
 *    límite del entorno, no del diseño, pero hace la opción insegura.
 *  - Se usa un **atlas**: una sola textura 2D con todas las texturas del
 *    nivel dentro, cada una en su sitio y con **un texel de borde
 *    replicado** para que el filtrado bilineal no mezcle una textura con la
 *    vecina. El shader calcula la posición del texel dentro de la textura y
 *    la traduce a la del atlas.
 *
 * Consecuencias (documentadas a propósito):
 *  - El recurso del atlas tiene el tamaño de la suma de las texturas (con
 *    una fila por textura, o varias filas si no caben); las imágenes
 *    originales se conservan intactas y con sus dimensiones (están en
 *    `gex_vfx_texture`).
 *  - El índice de textura del `.VFX` (14 bits) no es el número de
 *    textura del atlas: el shader busca el índice en una tabla constante (el
 *    conjunto del nivel tiene pocos índices) y de ahí sale la posición y el
 *    tamaño.
 *  - Si una textura no se pudo decodificar, su índice **no** está en la
 *    tabla y el shader dibuja ese polígono con el color por vértice, sin
 *    inventar ninguna textura.
 *
 * La preparación (decodificación) es portable y vive en
 * `gex/vfx_texture.h`; aquí solo se crean y liberan los objetos COM.
 */

#ifndef GEX_RENDER_D3D11_TEXTURE_H
#define GEX_RENDER_D3D11_TEXTURE_H

#if defined(_WIN32)

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#ifndef COBJMACROS
#define COBJMACROS
#endif
#include <windows.h>

#include <d3d11.h>

#include <stdint.h>

#include "gex/render/d3d11_device.h"
#include "gex/vfx_texture.h"

#ifdef __cplusplus
extern "C" {
#endif

/* Modo de interpretación de la coordenada de textura.
 *
 * GEX_NCC_UV_MODE_NORMALIZED: el byte de UV del `.DFX` es una fracción de
 *   la textura (256 bytes = la textura entera). Es la lectura que encaja con
 *   el hecho de que el divisor del original sea una constante (independiente
 *   del tamaño de la textura) y con que las esquinas de los polígonos caigan
 *   en 0 y 255.
 *
 * GEX_NCC_UV_MODE_TEXEL: el byte de UV es la coordenada en texels, y la
 *   textura se repite cada `w` x `h` texels.
 *
 * El modo se elige al crear el conjunto y se aplica en el shader, así que se
 * puede cambiar sin tocar la geometría.
 */
typedef enum gex_ncc_uv_mode {
    GEX_NCC_UV_MODE_NORMALIZED = 0,
    GEX_NCC_UV_MODE_TEXEL      = 1
} gex_ncc_uv_mode;

/* Máximo de entradas en la tabla de búsqueda (índice de textura -> atlas). */
#define GEX_D3D11_TEXTURE_MAX_ENTRIES 64

/* Ancho máximo del atlas antes de partir en más filas. */
#define GEX_D3D11_TEXTURE_ATLAS_MAX_WIDTH 4096u

typedef enum gex_d3d11_texture_status {
    GEX_D3D11_TEXTURE_OK = 0,
    GEX_D3D11_TEXTURE_BAD_ARGUMENT,   /* device/banco nulos o vacíos */
    GEX_D3D11_TEXTURE_BAD_STATE,      /* conjunto ya inicializado */
    GEX_D3D11_TEXTURE_TOO_MANY,       /* más entradas que el máximo */
    GEX_D3D11_TEXTURE_CREATE_FAILED,  /* falló un Create* */
    GEX_D3D11_TEXTURE_OUT_OF_MEMORY
} gex_d3d11_texture_status;

/*
 * Conjunto de texturas: un atlas 2D con todas las texturas del nivel, su
 * shader resource view, el sampler (lineal + wrap, los valores por defecto
 * de Glide 2.x) y el constant buffer con la tabla de búsqueda.
 */
typedef struct gex_d3d11_texture_set {
    ID3D11Device *device;                /* prestado (R-1) */
    ID3D11Texture2D *atlas;
    ID3D11ShaderResourceView *srv;
    ID3D11SamplerState *sampler;
    ID3D11Buffer *lookup_buffer;         /* constant buffer b1 */
    UINT   texture_count;                /* texturas en el atlas */
    UINT   atlas_width;
    UINT   atlas_height;
    UINT   entry_count;                  /* entradas de la tabla */
    gex_ncc_uv_mode uv_mode;
    HRESULT last_hresult;
    int     initialized;
} gex_d3d11_texture_set;

#define GEX_D3D11_TEXTURE_SET_INITIALIZER                                   \
    {                                                                       \
        .device = NULL, .atlas = NULL, .srv = NULL, .sampler = NULL,         \
        .lookup_buffer = NULL, .texture_count = 0, .atlas_width = 0,        \
        .atlas_height = 0, .entry_count = 0,                                \
        .uv_mode = GEX_NCC_UV_MODE_NORMALIZED,                              \
        .last_hresult = S_OK, .initialized = 0                              \
    }

/*
 * Crea el conjunto a partir de un banco de texturas portable.
 *
 *   device/banco nulos, o banco sin texturas -> BAD_ARGUMENT
 *   conjunto ya inicializado                  -> BAD_STATE
 *   más de GEX_D3D11_TEXTURE_MAX_ENTRIES      -> TOO_MANY
 *   fallo de Create*                          -> CREATE_FAILED
 */
gex_d3d11_texture_status gex_d3d11_texture_set_create(
    gex_d3d11_texture_set *set, const gex_d3d11_device *device,
    const gex_vfx_texture_bank *bank, gex_ncc_uv_mode uv_mode);

/* Libera todos los objetos COM y deja la estructura a cero. Idempotente. */
void gex_d3d11_texture_set_destroy(gex_d3d11_texture_set *set);

const char *gex_d3d11_texture_status_name(gex_d3d11_texture_status status);

#ifdef __cplusplus
}
#endif

#endif /* _WIN32 */

#endif /* GEX_RENDER_D3D11_TEXTURE_H */
