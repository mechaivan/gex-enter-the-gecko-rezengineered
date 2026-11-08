/*
 * gex/bsp.h - BSP traversal for level geometry (S-2).
 *
 * Evidence: U1H_RESULTADOS.md §6.1 (0x00401C00): pila explicita, nodo 0x20 B,
 *   centro en +0x00/+0x02/+0x04 (int16), radio en +0x06 (int16), tipo en +0x12
 *   (bit 1 = hoja), hijos en +0x18/+0x1C (offsets relativos a la imagen).
 *   Anexa hojas a una lista con tope 0x800.
 *
 * El recorrido estructural se implementa aqui. La prueba de visibilidad
 * (esfera vs. volumen de vista) depende de la camara (cam+0x88, +0x5C, planos
 * en +0xC8/+0xD4/+0xD8) y NO se reproduce en esta version: se inyecta como
 * predicado opcional. Con predicado NULL se aceptan todas las hojas (util para
 * verificar la estructura, no la visibilidad original).
 */
#ifndef GEX_BSP_H
#define GEX_BSP_H

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define GEX_BSP_NODE_SIZE    32u
#define GEX_BSP_LEAF_FLAG    0x0002u
#define GEX_BSP_LIST_CAP     0x800u

typedef enum gex_bsp_status {
    GEX_BSP_OK = 0,
    GEX_BSP_ERR_ARGUMENT,
    GEX_BSP_ERR_BOUNDS,
    GEX_BSP_ERR_CYCLE,
    GEX_BSP_ERR_MEMORY,
    GEX_BSP_ERR_OVERFLOW
} gex_bsp_status;

typedef struct gex_bsp_walk_result {
    uint32_t *leaves;   /* offsets de nodo (relativos a la imagen) */
    uint32_t count;
    uint32_t capacity;
} gex_bsp_walk_result;

/* Predicado de visibilidad: !=0 si el nodo es visible. NULL => aceptar todo. */
typedef int (*gex_bsp_visible_fn)(const uint8_t *image, size_t image_size,
                                  uint32_t node_offset, void *ctx);

gex_bsp_status gex_bsp_walk(const uint8_t *image, size_t image_size,
                            uint32_t root_offset,
                            gex_bsp_visible_fn visible, void *ctx,
                            gex_bsp_walk_result *out);

void gex_bsp_walk_free(gex_bsp_walk_result *r);

/* Ayudantes de lectura de nodo (offsets relativos a la imagen). */
uint16_t gex_bsp_node_type(const uint8_t *image, size_t image_size,
                           uint32_t node_offset);
int      gex_bsp_node_is_leaf(const uint8_t *image, size_t image_size,
                              uint32_t node_offset);

#ifdef __cplusplus
}
#endif

#endif /* GEX_BSP_H */
