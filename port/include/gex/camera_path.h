/*
 * gex/camera_path.h - camera path (spline) evaluation of the original
 * GEX3D.EXE (portable).
 *
 * Reproduces, with the original integer arithmetic, the three routines
 * measured on the retail executable (see S3_MENU_CAMERA_SEQUENCE.md
 * sections 4.9-4.14):
 *
 *   0x53F252  advance the node        index + 15-bit fraction, +1 per frame
 *   0x53EF00  evaluate one sample     direct copy, or Hermite
 *   0x53DEB0  Hermite in Q15          integer only, 32-bit wrapping
 *
 * The arithmetic is the original one, not an approximation:
 *
 *   - the node keeps the raw 16-bit word at `+2`: bit 0 is a mode bit
 *     that the advance preserves, bits 1..15 are the 15-bit fraction;
 *   - the advance adds 1 to the fraction modulo 0x7FFF and, when the
 *     fraction reaches the segment duration D, moves to the next index
 *     and clears the fraction;
 *   - the last index is the sentinel: D == 0, so the sample is the
 *     stored point and no further sample is produced;
 *   - the evaluation uses signed division truncated toward zero, 32-bit
 *     wrapping products, arithmetic `sar 15` shifts and a final
 *     truncation to int16.
 *
 * No floating point is used anywhere.
 *
 * Scope: variant 0 (32-byte keys: point + in/out tangents), which is the
 * camera *position* path of MAP7. Variant 1 (10-byte keys, rotation,
 * 0x43158D / 0x431036) is NOT modelled: the functions reject it instead
 * of guessing its arithmetic.
 *
 * Ingenieria inversa estatica y de solo lectura. No se modifican
 * ejecutables ni assets originales.
 */

#ifndef GEX_CAMERA_PATH_H
#define GEX_CAMERA_PATH_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Serialised key of the 32-byte variant (offset inside the key). */
#define GEX_CAMERA_PATH_KEY_SIZE 32
#define GEX_CAMERA_PATH_KEY_DURATION   0x00 /* int16 */
#define GEX_CAMERA_PATH_KEY_POINT      0x02 /* int16[3] */
#define GEX_CAMERA_PATH_KEY_TANGENT_OUT 0x08 /* int32[3] */
#define GEX_CAMERA_PATH_KEY_TANGENT_IN  0x14 /* int32[3] */

/* Fraction / interpolation constants (S = [0x0054C1B0] = 15). */
#define GEX_CAMERA_PATH_FRAC_BITS 15
#define GEX_CAMERA_PATH_FRAC_MASK 0x7FFFu
#define GEX_CAMERA_PATH_SHIFT 15
#define GEX_CAMERA_PATH_ONE (1 << GEX_CAMERA_PATH_SHIFT)

/* `variant` byte at object +0x06. */
#define GEX_CAMERA_PATH_VARIANT_POSITION 0
#define GEX_CAMERA_PATH_VARIANT_ROTATION 1

/* Flags byte at object +0x07: bits 1 and 2 send the index back to 0 when
 * the end is reached (measured; their meaning is not established). */
#define GEX_CAMERA_PATH_FLAG_WRAP_A 0x02
#define GEX_CAMERA_PATH_FLAG_WRAP_B 0x04

/* One 32-byte key of the path. `duration` is the number of frames of the
 * segment that starts at this key. */
typedef struct gex_camera_path_key {
    int16_t duration;
    int16_t point[3];
    int32_t tangent_out[3];
    int32_t tangent_in[3];
} gex_camera_path_key;

/* The path object: `obj+0` (keys), `obj+4` (count), `obj+6` (variant) and
 * `obj+7` (flags) in the original. The keys are already validated by the
 * caller (the port does not parse the DFX here). */
typedef struct gex_camera_path {
    const gex_camera_path_key *keys;
    int16_t count;
    uint8_t variant;
    uint8_t flags;
} gex_camera_path;

/* The node: `node+0` (index) and `node+2` (raw fraction word) in the
 * original. Initialise it with gex_camera_path_init_node(). */
typedef struct gex_camera_path_node {
    int16_t index;
    uint16_t fraction;
} gex_camera_path_node;

/* index = 0, fraction = 0: the state the original starts from. */
void gex_camera_path_init_node(gex_camera_path_node *node);

/* 15-bit fraction stored in the raw word (bits 1..15). */
uint16_t gex_camera_path_fraction(const gex_camera_path_node *node);

/* Mode bit stored in the raw word (bit 0). It is 0 for the MAP7 camera
 * path; the code reproduces both branches. */
uint16_t gex_camera_path_mode(const gex_camera_path_node *node);

/* 0x53F252. Advances the node by one frame.
 * Returns 1 when the caller must evaluate a sample, 0 when it must keep
 * the previous position (invalid arguments, end of the path, or the
 * variant this module does not model). */
int gex_camera_path_advance(const gex_camera_path *path,
                            gex_camera_path_node *node);

/* 0x53EF00. Evaluates the node into `out` (three int16).
 * Returns 1 when a sample was produced, 0 otherwise. `out` is not
 * touched when the function returns 0. */
int gex_camera_path_eval(const gex_camera_path *path,
                         const gex_camera_path_node *node,
                         int16_t out[3]);

/* advance + eval, the pair the original calls per frame (0x53E997 and
 * 0x53E9B0). Returns 1 and writes `position` only when a sample was
 * produced; otherwise `position` is left untouched. */
int gex_camera_path_step(const gex_camera_path *path,
                         gex_camera_path_node *node,
                         int16_t position[3]);

/* Contract of the validated records this module expects: non-NULL keys,
 * at least two of them, variant 0 and a sentinel at the end (last
 * duration 0). Returns 1 when the path is usable. */
int gex_camera_path_validate(const gex_camera_path *path);

#ifdef __cplusplus
}
#endif

#endif /* GEX_CAMERA_PATH_H */
