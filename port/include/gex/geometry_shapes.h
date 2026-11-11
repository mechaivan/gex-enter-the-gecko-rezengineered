/*
 * gex/geometry_shapes.h - geometrías de prueba reutilizables (portable).
 *
 * Produce `gex_emit_output` listo para `gex_geometry_build` y
 * `gex_d3d11_mesh_create_from_emit`, con las mismas estructuras que usa la
 * emisión real de S-2: nada de formatos paralelos.
 *
 * El cubo está pensado como la primera geometría 3D visible del port:
 *
 *   - se dibuja en el espacio lógico de la emisión (512x384, el mismo del
 *     original: centro en 256/192 y puerta de pantalla en 512x384);
 *   - cada cara tiene su color, sombreado por su normal (Lambert);
 *   - cada vértice lleva su profundidad, de forma que el orden de emisión es
 *     el orden del pintor (de atrás hacia delante), igual que el original;
 *   - los flags son los de la emisión (`GEX_EMIT_F_NEAR_EMITTER`), sin
 *     niebla y sin texturas.
 *
 * Portable C11: sin Windows, sin D3D11.
 */

#ifndef GEX_GEOMETRY_SHAPES_H
#define GEX_GEOMETRY_SHAPES_H

#include "gex/emit.h"

#ifdef __cplusplus
extern "C" {
#endif

/* Triángulos que emite el cubo: 6 caras x 2 triángulos. */
#define GEX_SHAPE_CUBE_TRIANGLES 12

/*
 * Emite un cubo girado `angle_radians` alrededor del eje Y (una vuelta
 * completa son 2*pi radianes) y con una inclinación fija de 0,35 radianes
 * alrededor de X, para que se vean tres caras a la vez.
 *
 * `out` debe estar a cero (o ser una salida nueva): la función reserva la
 * lista de triángulos y la devuelve lista para `gex_emit_output_free`.
 *
 * Devuelve 0 en éxito y -1 si no hay memoria (en ese caso `out` queda
 * vacío). Es determinista: mismos parámetros, mismos triángulos.
 */
int gex_shape_cube(gex_emit_output *out, float angle_radians);

#ifdef __cplusplus
}
#endif

#endif /* GEX_GEOMETRY_SHAPES_H */
