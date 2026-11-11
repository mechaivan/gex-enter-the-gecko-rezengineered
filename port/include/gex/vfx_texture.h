/*
 * gex/vfx_texture.h - banco de texturas decodificadas de un `.VFX` (portable).
 *
 * Complementa a `gex/vfx.h` (Módulo 4A) y a `gex/vfx_decode.c` (Módulo 4B):
 * carga un `.VFX`, decodifica a RGBA8 las entradas que el nivel usa de
 * verdad y las deja accesibles por el índice de textura de 14 bits.
 *
 * Es la parte **CPU y portable** de la integración de texturas: aquí no hay
 * D3D11 ni Win32, solo memoria y decodificación. La capa de recursos
 * (`gex/render/d3d11_texture.h`) es la que crea los objetos COM.
 *
 * Reglas:
 *   - Solo se decodifican los formatos con decodificador (YIQ_422, ARGB_1555
 *     y ARGB_4444). Una entrada con otro formato NO se decodifica: se cuenta
 *     como fallida y no se inventa ningún contenido.
 *   - Una entrada fallida no se sustituye por un color liso ni por datos
 *     inventados: simplemente no hay textura para ese índice.
 *   - Las dimensiones originales se conservan: cada textura tiene su ancho y
 *     alto reales (el del `GrTexInfo` del `.VFX`).
 */

#ifndef GEX_VFX_TEXTURE_H
#define GEX_VFX_TEXTURE_H

#include <stddef.h>
#include <stdint.h>

#include "gex/vfx.h"

#ifdef __cplusplus
extern "C" {
#endif

/* Una textura decodificada: RGBA8, 4 bytes por píxel (R, G, B, A) con alfa
 * recto. `rgba` es propiedad del banco. */
typedef struct gex_vfx_texture {
    uint32_t index;   /* índice de textura de 14 bits del `.VFX` */
    uint32_t format;  /* GR_TEXFMT del registro */
    uint32_t width;   /* ancho original en texels */
    uint32_t height;  /* alto original en texels */
    uint8_t *rgba;    /* width * height * 4 bytes, o NULL si no se pudo */
} gex_vfx_texture;

/* Banco de texturas de un `.VFX`. */
typedef struct gex_vfx_texture_bank {
    gex_vfx_file vfx;
    int          vfx_loaded;
    gex_vfx_texture *textures;  /* una entrada por textura decodificada */
    uint32_t     texture_count;
    uint32_t    *slots;         /* índice de 14 bits -> ranura, o UINT32_MAX */
    uint32_t     slot_count;    /* número de ranuras (índice 14 bits + 1) */
    uint32_t     max_width;     /* ancho máximo de las texturas cargadas */
    uint32_t     max_height;    /* alto máximo de las texturas cargadas */
    uint32_t     decoded_count; /* texturas decodificadas con éxito */
    uint32_t     failed_count;  /* entradas pedidas que no se pudieron decodificar */
} gex_vfx_texture_bank;

/*
 * Carga el `.VFX` de `vfx_path` y decodifica las entradas cuyos índices
 * están en `wanted` (array de `wanted_count` índices de 14 bits; puede ser
 * NULL si no se quiere ninguna).
 *
 * Los índices repetidos se ignoran y los que no existen en el `.VFX` se
 * cuentan como fallidos (no hay textura para ellos).
 *
 *   bank / vfx_path nulo, o lista vacía -> GEX_VFX_ERR_ARG
 *   el `.VFX` no se puede cargar           -> el estado devuelto
 *   Decodificación fallida de una entrada  -> la entrada queda sin textura y
 *                                             se cuenta en `failed_count`
 */
gex_vfx_status gex_vfx_texture_bank_load(gex_vfx_texture_bank *bank,
                                         const char *vfx_path,
                                         const uint32_t *wanted,
                                         uint32_t wanted_count);

/* Textura decodificada de un índice de 14 bits, o NULL si no hay ninguna. */
const gex_vfx_texture *gex_vfx_texture_bank_find(
    const gex_vfx_texture_bank *bank, uint32_t index);

/* Ranura (0..texture_count-1) de un índice de 14 bits, o -1 si no hay
 * textura para ese índice. La ranura es el índice dentro de `textures`. */
int gex_vfx_texture_bank_slot(const gex_vfx_texture_bank *bank, uint32_t index);

/* Libera todo. Idempotente. */
void gex_vfx_texture_bank_free(gex_vfx_texture_bank *bank);

#ifdef __cplusplus
}
#endif

#endif /* GEX_VFX_TEXTURE_H */
