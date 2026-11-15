/*
 * gex/actor.h - clases y modelos de actor de un nivel `.DFX` (portable).
 *
 * Un nivel de GEX no solo tiene BSP: también tiene **actores** (el televisor
 * del menú, los botones, el propio Gex...). Este módulo lee del contenedor
 * `.DFX` la parte que describe un actor:
 *
 *   clase   -> nombre, número de modelos y tabla de modelos
 *   modelo  -> vértices, polígonos, subpartes, normales y radio
 *
 * Offsets medidos sobre MAP7.DFX (S-4, ver
 * docs/S3_MENU_CAMERA_SEQUENCE.md § 11):
 *
 *   clase +0x08  número de modelos (uint32)
 *   clase +0x0C  tabla de punteros a modelo (uint32[n])
 *   clase +0x10  campo usado por el selector de rama de dibujo
 *                (0 = rama de panel 0x4718CF; != 0 = rama texturizada
 *                0x470F96)
 *   clase +0x14  par de int16 (escala del actor)
 *   clase +0x18  par de int16 (escala del actor)
 *   clase +0x24  puntero al nombre (cadena NUL, hasta 31 caracteres)
 *
 *   modelo +0x00  número de vértices (uint32)
 *   modelo +0x04  tabla de vértices (offset en la imagen)
 *   modelo +0x10  número de polígonos (uint32)
 *   modelo +0x14  tabla de polígonos (offset en la imagen)
 *   modelo +0x18  número de subpartes (uint32)
 *   modelo +0x1C  tabla de subpartes (offset en la imagen)
 *   modelo +0x20  tabla de normales (offset; 0 = sin normales)
 *   modelo +0x24  radio (uint32)
 *
 *   vértice  = 12 B: { int16 x, y, z, shade, 0, 0 }
 *   polígono = 12 B: { uint16 i0, i1, i2, uint16 flags, uint32 uv_ptr }
 *   subparte = 24 B: { int16 +0x00..+0x05 (sin usar por el emisor),
 *                      int16 +0x06 último polígono,
 *                      int16 +0x08 primer vértice,
 *                      int16 +0x0A último vértice,
 *                      ... }
 *
 * Los offsets del `.DFX` son **offsets de imagen** (`gex_dfx_container.image`),
 * no del fichero: `fichero = imagen + container->aligned * 4`.
 *
 * El registro UV del polígono de actor es el mismo de 12 B que ya documenta
 * `gex/emit.h`: tres grupos `{ u8 u, u8 v, u16 texture }`, con
 * `texture = word[+6] & 0x3FFF` y `layer = int8[+0x0B]`.
 *
 * Portable C11: sin Windows, sin D3D11, sin Win32.
 */

#ifndef GEX_ACTOR_H
#define GEX_ACTOR_H

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Tamaños de registro medidos en MAP7.DFX. */
#define GEX_ACTOR_CLASS_NAME_MAX   32u
#define GEX_ACTOR_VERTEX_SIZE      12u
#define GEX_ACTOR_POLYGON_SIZE     12u
#define GEX_ACTOR_SUBPART_SIZE     24u
#define GEX_ACTOR_MODEL_SIZE       0x30u
#define GEX_ACTOR_UV_SIZE          12u

typedef enum gex_actor_status {
    GEX_ACTOR_OK = 0,
    GEX_ACTOR_ERR_ARG,        /* puntero nulo */
    GEX_ACTOR_ERR_RANGE,      /* offset o índice fuera de la imagen */
    GEX_ACTOR_ERR_FORMAT,     /* la estructura no cumple el contrato */
    GEX_ACTOR_ERR_NAME        /* el nombre no es una cadena UTF-8/Latin-1 válida */
} gex_actor_status;

/* Subparte de un modelo: el emisor la usa para saltar polígonos y para
 * acotar el rango de vértices de un billboard. */
typedef struct gex_actor_subpart {
    int16_t last_polygon;  /* +0x06: -1 = subparte sin dibujo */
    int16_t first_vertex;  /* +0x08 */
    int16_t last_vertex;   /* +0x0A */
} gex_actor_subpart;

/* Descriptor de modelo (0x30 B en la imagen). */
typedef struct gex_actor_model {
    uint32_t offset;          /* offset del descriptor en la imagen */
    uint32_t vertex_count;
    uint32_t vertex_table;
    uint32_t polygon_count;
    uint32_t polygon_table;
    uint32_t subpart_count;
    uint32_t subpart_table;
    uint32_t normal_table;    /* 0 = sin normales */
    uint32_t radius;
} gex_actor_model;

/* Clase de actor. */
typedef struct gex_actor_class {
    uint32_t offset;
    char     name[GEX_ACTOR_CLASS_NAME_MAX];
    uint32_t model_count;
    uint32_t model_table;
    uint32_t field_0x10;      /* selector de rama de dibujo */
} gex_actor_class;

/* Polígono de actor. */
typedef struct gex_actor_polygon {
    uint16_t vtx[3];
    uint16_t flags;
    uint32_t uv_ptr;          /* offset del registro UV en la imagen */
} gex_actor_polygon;

/* Registro UV de un polígono de actor (12 B). */
typedef struct gex_actor_uv {
    uint8_t  u[3];
    uint8_t  v[3];
    uint16_t texture;         /* 14 bits */
    int8_t   layer;
} gex_actor_uv;

/*
 * Lee la clase que empieza en `offset` (offset de imagen). Valida el
 * contrato mínimo (tabla de modelos dentro de la imagen, nombre imprimible)
 * y rellena `out`. No adivina: ante cualquier incoherencia devuelve error.
 */
gex_actor_status gex_actor_class_read(const uint8_t *image, size_t image_size,
                                      uint32_t offset, gex_actor_class *out);

/*
 * Busca en la imagen una clase cuyo nombre sea exactamente `name`.
 *
 * Recorre la imagen en pasos de 4 bytes buscando candidatos con el mismo
 * contrato que `gex_actor_class_read` y compara el nombre. Es una búsqueda
 * acotada (la imagen de MAP7 son ~900 KB) y solo se usa cuando el llamador no
 * conoce el offset; el camino normal es `gex_actor_class_read`.
 *
 * Devuelve GEX_ACTOR_ERR_NAME si no aparece.
 */
gex_actor_status gex_actor_class_find(const uint8_t *image, size_t image_size,
                                      const char *name, gex_actor_class *out);

/* Lee el modelo `index` de la clase. */
gex_actor_status gex_actor_model_read(const uint8_t *image, size_t image_size,
                                      const gex_actor_class *cls, uint32_t index,
                                      gex_actor_model *out);

/* Vértice `index` del modelo: tres int16 (x, y, z). */
gex_actor_status gex_actor_vertex(const uint8_t *image, size_t image_size,
                                  const gex_actor_model *model, uint32_t index,
                                  int16_t xyz[3]);

/* Polígono `index` del modelo. */
gex_actor_status gex_actor_read_polygon(const uint8_t *image, size_t image_size,
                                   const gex_actor_model *model, uint32_t index,
                                   gex_actor_polygon *out);

/* Subparte `index` del modelo. */
gex_actor_status gex_actor_read_subpart(const uint8_t *image, size_t image_size,
                                   const gex_actor_model *model, uint32_t index,
                                   gex_actor_subpart *out);

/* Registro UV del polígono `poly`. */
gex_actor_status gex_actor_read_uv(const uint8_t *image, size_t image_size,
                              const gex_actor_polygon *poly, gex_actor_uv *out);

/*
 * ¿El polígono tiene un registro UV utilizable? El panel del televisor
 * (`tvmenu__`) lleva `uv_ptr = 0x20800080`, que no cae dentro de la imagen:
 * es el marcador de "polígono sin textura" que ya se midió en S-4. Esta
 * función devuelve 0 en ese caso, sin inventar una textura.
 */
int gex_actor_polygon_textured(const uint8_t *image, size_t image_size,
                               const gex_actor_polygon *poly);

const char *gex_actor_status_name(gex_actor_status status);

#ifdef __cplusplus
}
#endif

#endif /* GEX_ACTOR_H */
