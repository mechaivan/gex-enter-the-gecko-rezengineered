/*
 * gex/asset/dfx.h - reader for the ".DFX" container of GEX3D.EXE (EU).
 */
#ifndef GEX_ASSET_DFX_H
#define GEX_ASSET_DFX_H

#include <stddef.h>
#include <stdint.h>

#include "gex/camera_path.h"

#ifdef __cplusplus
extern "C" {
#endif

#define GEX_DFX_ALIGN_MASK 0x1FFu
#define GEX_DFX_ALIGN_ADD  0x200u

#define GEX_DESCRIPTOR_SIZE 64u
#define GEX_VERTEX_SIZE     12u
#define GEX_POLYGON_SIZE    20u
#define GEX_NORMAL_SIZE     6u

/*
 * Trayectoria de cámara dentro del contenedor.
 *
 * La cámara del menú (MAP7) no está en el código: está en el `.DFX` del nivel,
 * en la **imagen compactada** (la misma región que `gex_dfx_container.image`
 * expone). Los offsets de abajo son offsets **dentro de esa imagen**, no
 * direcciones de runtime: el juego los recorre con punteros que el loader
 * reubica, pero el contenido del fichero ya está construido.
 *
 * Cadena medida (S3_MENU_CAMERA_SEQUENCE.md § 4.5):
 *
 *   cabecera de nivel 0x5FC  ->  slot 0 (opcode 0x4F) -> registro de actor
 *   actor + 0x28             ->  entidad
 *   entidad + 0x00           ->  objeto de la trayectoria
 *   objeto + 0x00            ->  array de registros de 32 B
 *   objeto + 0x04            ->  cantidad de registros (uint16)
 *   objeto + 0x06            ->  variante (uint8): 0 = posición (32 B)
 *   objeto + 0x07            ->  flags (uint8)
 *
 * El opcode `0x4F` y el criterio que elige el slot 0 **no** están
 * identificados: lo que está medido es que el slot 0 es el que lleva la
 * trayectoria de posición y que sus bytes coinciden con el array que el
 * original tiene en memoria. El cargador valida cada paso de la cadena y
 * rechaza el nivel si algo no cuadra (no adivina).
 */
#define GEX_DFX_LEVEL_HEADER_OFFSET  0x5FCu
#define GEX_DFX_ACTOR_ENTITY_OFFSET  0x28u
#define GEX_DFX_SLOT_OPCODE_PATH     0x4Fu
#define GEX_DFX_SLOT_OPCODE_END      0x0Eu
#define GEX_DFX_SLOT_CAMERA          0u  /* trayectoria de POSICION */
#define GEX_DFX_SLOT_CAMERA_LOOKAT   1u  /* trayectoria de MIRA (look-at) */
#define GEX_DFX_MAX_SLOTS            16u

typedef enum gex_dfx_status {
    GEX_DFX_OK = 0,
    GEX_DFX_ERR_ARGUMENT,
    GEX_DFX_ERR_IO,
    GEX_DFX_ERR_FORMAT,
    GEX_DFX_ERR_TRUNCATED,
    GEX_DFX_ERR_ALIGNMENT,
    GEX_DFX_ERR_POINTER,
    GEX_DFX_ERR_MEMORY,
    GEX_DFX_ERR_DESCRIPTOR,
    GEX_DFX_ERR_OVERFLOW,
    GEX_DFX_ERR_PATH       /* no hay trayectoria de cámara, o no es válida */
} gex_dfx_status;

typedef struct gex_dfx_container {
    uint32_t count;
    uint32_t *offsets;
    uint8_t *image;
    size_t image_size;
    size_t data_size;
    uint32_t aligned;
} gex_dfx_container;

typedef struct gex_dfx_descriptor {
    uint32_t root_bsp;
    uint32_t field_0x04;
    uint32_t field_0x08;
    uint32_t field_0x0C;
    uint32_t field_0x10;
    uint32_t field_0x14;
    uint32_t vertex_count;
    uint32_t polygon_count;
    uint32_t normal_count;
    uint32_t vertex_table;
    uint32_t polygon_table;
    uint32_t normal_table;
    uint32_t object_table;
    uint32_t field_0x34;
    uint32_t field_0x38;
    uint32_t field_0x3C;
} gex_dfx_descriptor;

typedef struct gex_dfx_level {
    gex_dfx_descriptor descriptor;
    uint8_t *vertex_data;
    uint8_t *polygon_data;
    int16_t *normal_data;
} gex_dfx_level;

/*
 * Trayectoria de cámara leída del contenedor.
 *
 * `path` es la vista que consume `gex_camera_path_*`: sus `keys` son propiedad
 * del llamador (los libera `gex_dfx_release_camera_path`). Los offsets son los
 * de la cadena recorrida, para poder contrastar la lectura con el fichero sin
 * volver a recorrerlo.
 */
typedef struct gex_dfx_camera_path {
    gex_camera_path path;   /* keys + count + variant + flags */
    /* Puntero de propiedad del array de registros (la vista `path.keys` es
     * `const` porque el modulo de camara no lo modifica). Lo libera
     * `gex_dfx_release_camera_path`. */
    void *owned_keys;
    uint32_t header_offset;
    uint32_t actor_offset;
    uint32_t entity_offset;
    uint32_t object_offset;
    uint32_t keys_offset;
    int loaded;
} gex_dfx_camera_path;

gex_dfx_status gex_dfx_parse(const void *data, size_t size, gex_dfx_container *out);
gex_dfx_status gex_dfx_load(const char *path, gex_dfx_container *out);
gex_dfx_status gex_dfx_relocate(gex_dfx_container *container, uint32_t delta);
gex_dfx_status gex_dfx_relocate_raw(uint8_t *image, size_t image_size,
                                    const uint32_t *offsets, uint32_t count,
                                    uint32_t delta);
gex_dfx_status gex_dfx_load_level(const gex_dfx_container *container,
                                  gex_dfx_level *out);

/*
 * Lee la trayectoria de cámara del contenedor siguiendo la cadena de offsets
 * documentada (ver las constantes de arriba). Devuelve GEX_DFX_ERR_PATH si el
 * nivel no tiene esa estructura o si los registros no cumplen el contrato
 * (variante 0, duraciones no negativas y centinela `D == 0` al final).
 *
 * En éxito `out->keys` es un array de `out->count` registros de 32 B
 * mallocado por el módulo; el llamador lo libera con
 * `gex_dfx_release_camera_path`.
 */
gex_dfx_status gex_dfx_load_camera_path(const gex_dfx_container *container,
                                        gex_dfx_camera_path *out);

/*
 * Igual que `gex_dfx_load_camera_path`, pero eligiendo el slot de la lista
 * de la cabecera de nivel. MAP7 tiene dos entradas con opcode 0x4F:
 *
 *   slot 0 -> actor 0x95EC -> ... -> array 0xC034 (POSICION)
 *   slot 1 -> actor 0x961C -> ... -> array 0xC39C (MIRA)
 *
 * `gex_dfx_load_camera_path` es exactamente `slot == 0`. El slot 1 es la
 * segunda trayectoria del look-at (0x406515 -> 0x40AE51). Se valida la
 * misma cadena y el mismo contrato de registros; si el slot no existe o
 * no cumple, se devuelve GEX_DFX_ERR_PATH sin adivinar.
 */
gex_dfx_status gex_dfx_load_camera_path_slot(const gex_dfx_container *container,
                                             uint32_t slot,
                                             gex_dfx_camera_path *out);
void gex_dfx_release_container(gex_dfx_container *container);
void gex_dfx_release_level(gex_dfx_level *level);
void gex_dfx_release_camera_path(gex_dfx_camera_path *path);
const char *gex_dfx_status_name(gex_dfx_status status);

#ifdef __cplusplus
}
#endif

#endif /* GEX_ASSET_DFX_H */
