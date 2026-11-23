/*
 * gex/comp2d.h - compositor 2D portable (S-13).
 *
 * El motor original **no** tiene una sola rutina 2D. La reconstrucción
 * estática de S-13 (docs/D3D11_FIRST_SCENE_PLAN.md § 29) documenta cinco
 * rutinas (agrupadas aquí en cuatro familias), y este módulo reproduce cada
 * una por separado:
 *
 *   - **Rectángulo sólido**: comando `0xF0` (constante `0x64FB40`), handler
 *     `0x00469268`. Normaliza las esquinas, descarta si `|dx| > 0x800` o
 *     `|dy| > 0x800` y emite **dos** `grDrawTriangle` sin textura. Es el
 *     fondo plano del vuelo (8, 9, 40) y también el de `0x00432DD1`, que
 *     además fija `grAlphaBlendFunction(0, 0, 0, 6)`.
 *   - **Cuadrilátero texturado**: comandos `0x2C` (0x2C B) y `0x24`
 *     (0x24 B). El índice de textura viaja en `word[+0x1A] & 0x3FFF`; los UV
 *     son bytes 0..255 (1/256 del lado mayor, S-5).
 *   - **Glifo**: `0x00432BBD` sobre `FONT.3DF`. Métrica medida:
 *     `x = xy & 0xFF`, `y = (xy >> 8) + 3`, `w = (wh & 0xFF) << 4`,
 *     `h = (wh >> 8) * 12`. El `+3` es del original.
 *   - **Línea / primitiva 2D**: `0x004FF06F`, opcode `0x40`, prefijo `0xE1`.
 *     Glide no tiene grosor: es una línea de 1 px; aquí se emite como un
 *     cuadrilátero fino para no depender de `D3D11_PRIMITIVE_TOPOLOGY_LINELIST`
 *     (el pipeline del port es una sola lista de triángulos).
 *
 * Además hay dos capas de conveniencia, que no son comandos del original pero
 * usan exactamente los mismos quads:
 *
 *   - `gex_comp2d_tiles`: una lista de tiles `(x, y, textura, w, h)`. Es la
 *     forma que tendrá el fondo del menú (tiles 325..365 de `MAP7.VFX`) en
 *     cuanto se recupere su tabla de colocación.
 *   - `gex_comp2d_vertical_gradient`: bandas horizontales sólidas a partir de
 *     paradas **medidas**. Es la aproximación documentada del degradado del
 *     menú mientras la tabla del fondo siga sin recuperarse (§ 29.4).
 *
 * Contrato:
 *   - Todo se emite en **píxeles lógicos** (0..511, 0..383) y `z = 0`: el
 *     pipeline no tiene depth buffer y dibuja en el orden del array, así que
 *     una capa 2D puesta delante se dibuja detrás de la geometría.
 *   - `GEX_COMP2D_NO_TEXTURE` (0x3FFF) no coincide con ninguna entrada del
 *     atlas: el shader devuelve el color por vértice sin muestrear nada.
 *   - Nada de D3D11, Win32 ni Glide: solo `gex/emit.h`.
 */

#ifndef GEX_COMP2D_H
#define GEX_COMP2D_H

#include <stddef.h>
#include <stdint.h>

#include "gex/emit.h"

#ifdef __cplusplus
extern "C" {
#endif

/* Índice de textura que el atlas nunca contiene (14 bits): rectángulo sólido.
 * El máximo real de texturas del `.VFX` es 16383 y el atlas guarda 256, así
 * que 0x3FFF es seguro y además es el centinela que ya usa el emisor. */
#define GEX_COMP2D_NO_TEXTURE 0x3FFFu

/* Rectángulo en píxeles lógicos. `w`/`h` son el tamaño en píxeles: la esquina
 * inferior derecha es `(x + w, y + h)`, igual que el comando `0xF0`. */
typedef struct {
    int16_t x, y;
    int16_t w, h;
} gex_comp2d_box;

/* Tile de un fondo 2D: posición en pantalla, textura y tamaño. Los tiles del
 * panorama de MAP7 son de 16x16 y 32x32 (§ 27.3). */
typedef struct {
    int16_t  x, y;
    uint16_t texture;
    uint8_t  width, height;
} gex_comp2d_tile;

/* Parada de un degradado vertical: fila lógica y color. */
typedef struct {
    int16_t y;
    uint8_t r, g, b;
} gex_comp2d_stop;

/* ------------------------------------------------------------------ */
/* Primitivas                                                          */
/* ------------------------------------------------------------------ */

/* Rectángulo sólido (2 triángulos, sin textura). Comando `0xF0`. */
gex_emit_status gex_comp2d_rect(gex_emit_output *out, gex_comp2d_box box,
                                uint32_t argb, uint8_t blend);

/* Cuadrilátero texturado (2 triángulos). `xy` en píxeles lógicos, `uv` en
 * bytes 0..255 como el original. Comandos `0x2C`/`0x24`. */
gex_emit_status gex_comp2d_quad(gex_emit_output *out, const int16_t xy[4][2],
                                const uint8_t uv[4][2], uint16_t texture,
                                const uint32_t argb[4], uint8_t blend);

/* Capa de tiles: un quad por tile con los UV de la textura entera. */
gex_emit_status gex_comp2d_tiles(gex_emit_output *out,
                                 const gex_comp2d_tile *tiles, size_t count,
                                 uint32_t argb, uint8_t blend);

/* Caja del glifo según la métrica de `0x00432BBD` (con el `+3` del original). */
gex_comp2d_box gex_comp2d_glyph_box(uint16_t glyph_xy, uint16_t glyph_wh);

/* Glifo texturado. `uv` lo pone el llamante: el original deriva los UV de la
 * posición del glifo dentro de `FONT.3DF`, que todavía no está descodificada. */
gex_emit_status gex_comp2d_glyph(gex_emit_output *out, uint16_t glyph_xy,
                                 uint16_t glyph_wh, const uint8_t uv[4][2],
                                 uint16_t texture, uint32_t argb,
                                 uint8_t blend);

/* Línea de `thickness` píxeles como cuadrilátero fino. Comando `0x40`. */
gex_emit_status gex_comp2d_line(gex_emit_output *out, int16_t x0, int16_t y0,
                                int16_t x1, int16_t y1, int16_t thickness,
                                uint32_t argb, uint8_t blend);

/* Degradado vertical por bandas sólidas entre paradas medidas. Las paradas no
 * necesitan estar ordenadas ni cubrir la caja entera: fuera del rango se
 * mantiene el color de la parada más cercana. */
gex_emit_status gex_comp2d_vertical_gradient(gex_emit_output *out,
                                             gex_comp2d_box box,
                                             const gex_comp2d_stop *stops,
                                             size_t stop_count,
                                             unsigned bands);

#ifdef __cplusplus
}
#endif

#endif /* GEX_COMP2D_H */
