/*
 * gex/camera.h - Camera orientation chain and visibility (S-2).
 *
 * Reproduces the original GEX3D.EXE camera pipeline statically reverse
 * engineered from the executable:
 *
 *   0x46DD40  build_orientation(cam)     -> O = M2 · R  (matrices by columns)
 *   0x46E643  orientation_finalize(dst,src) -> M = transpose(O)
 *   0x405C41  build_visibility_matrices(cam) -> A, B from M + local vectors
 *   0x402DC0  build_local_frame(cam)     -> corner rays + normals (approx norm)
 *   0x401C00  walk(desc, cam, out)       -> sphere vs 6 semiplanos culling
 *
 * All arithmetic is integer Q12 fixed-point (4096 = 1.0) with the same
 * operation order and `sar 12` steps as the original x86 code.
 *
 * The retail sine/cosine tables (BSS at 0x00646C20 / 0x00645C20) have no
 * statically located initializer, so the port generates them
 * mathematically in Q12. That generation was VERIFIED against the running
 * original on 2026-11-11: the retail table is the truncated value
 * trunc(4096*sin(2*pi*i/4096)) on all 1024 live entries (it differs from
 * the rounded value on 502 of them), so the port truncates. The lookup
 * keeps the original quadrant structure over a half-turn sine table.
 *
 * Ingeniería inversa estática y de solo lectura. No se modifican
 * ejecutables ni assets originales.
 */

#ifndef GEX_CAMERA_H
#define GEX_CAMERA_H

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Q12 fixed point: 4096 = 1.0, angles in fractions of a full turn. */
#define GEX_CAM_Q12 4096
#define GEX_CAM_ANGLE_MASK 0xFFF

/* Camera field offsets (verified against GEX3D.EXE). */
#define GEX_CAM_POS_X      0x00
#define GEX_CAM_POS_Y      0x02
#define GEX_CAM_POS_Z      0x04
#define GEX_CAM_EULER_X    0x30
#define GEX_CAM_EULER_Y    0x32
#define GEX_CAM_EULER_Z    0x34
#define GEX_CAM_FOCAL      0x40
#define GEX_CAM_WIDTH      0x48   /* int32 */
#define GEX_CAM_FAR        0x5C   /* int32, corte lejano */
#define GEX_CAM_ADVANCE    0x60   /* int16[3] */
#define GEX_CAM_NORMAL0    0x68   /* int16[3] */
#define GEX_CAM_NORMAL1    0x70   /* int16[3] */
#define GEX_CAM_NORMAL2    0x78   /* int16[3] */
#define GEX_CAM_NORMAL3    0x80   /* int16[3] */
#define GEX_CAM_MATRIX_A   0x88   /* int16[9] */
#define GEX_CAM_MATRIX_B   0xA8   /* int16[9] */
#define GEX_CAM_ACAM       0xC8   /* int32[3] = A · camPos */
#define GEX_CAM_BCAM       0xD4   /* int32[3] = B · camPos */
#define GEX_CAM_PTR_O      0xE4   /* ptr to O matrix (by columns) */
#define GEX_CAM_PTR_M      0xE8   /* ptr to M matrix = transpose(O) */
#define GEX_CAM_BASE       0xF0   /* ptr to base matrix or NULL */
#define GEX_CAM_FLAG_IDENT 0x43C  /* int32: != 0 forces base = identity */
#define GEX_CAM_ALT_ANGLES 0x128  /* int16[3] Euler angles, rama alternativa */

/* Global flag selecting normal vs alternative angle branch.
 * Original: [0x00551644 + 0xa0] & 0x8  (byte).
 * In the port, use gex_camera.use_alt_angles instead of reading a
 * hardcoded address. */

/* Matrix 3x3 Q12, row-major. w[3*r + c] = row r, column c. */
typedef struct {
    int16_t w[9];
} gex_cam_mat3;

/* Camera context: all fields the visibility/orientation chain needs.
 * The port owns this struct; the original game lays the same fields at
 * the same offsets inside its camera object. */
typedef struct {
    int16_t pos[3];            /* +0x00  world position */
    int16_t euler[3];          /* +0x30  normal Euler angles (Q12 turns) */
    int16_t focal;             /* +0x40  = 0x140 (320) */
    int16_t field_0x44;        /* +0x44  = 0x200 (unused by visibility) */
    int32_t width;             /* +0x48  = 0x180 (384) */
    int16_t local[5][3];       /* +0x60..0x84  built by build_local_frame */
    int16_t mat_a[9];          /* +0x88  matrix A (rows: advance, n0, n1) */
    int16_t pad_0x9a[13];      /* +0x9A  unused gap */
    int16_t mat_b[9];          /* +0xA8  matrix B (rows: n2, n3, ???) */
    int32_t a_cam[3];          /* +0xC8  A · camPos */
    int32_t b_cam[3];          /* +0xD4  B · camPos */
    gex_cam_mat3 m;            /* +0xE8  M = transpose(O) (embedded copy) */
    int32_t far_plane;               /* +0x5C  corte lejano (0x2EE0 = 12000) */
    int32_t flag_identity;     /* +0x43C != 0 => base = identity */
    int use_alt_angles;        /* != 0 => use cam->euler_alt instead of cam->euler */
    int16_t euler_alt[3];      /* +0x128 alternative angles */
    const gex_cam_mat3 *base;  /* +0xF0  base matrix (or NULL) */
    /* Escala por fila de la matriz de emision (0x46E340..0x46E448):
     * +0x490 escala la fila 0, +0x492 la fila 1 y +0x494 la fila 2.
     * El valor es un factor Q12: 0x1000 es la identidad. El juego los deja
     * a 0x1000 en el escenario de MAP7. */
    int16_t row_scale[3];
    /* Parametros de proyeccion que el juego fija una vez (0x4636DD..0x4636FB):
     * centro x (0x646814 = 256), centro y (0x646818 = 192) y plano cercano
     * (0x6474E8 = 160). No forman parte del bloque de camara, pero la
     * proyeccion los necesita junto con el. */
    int32_t proj_cx;
    int32_t proj_cy;
    int32_t near_z;
} gex_camera;

/* Matriz de emision: el bloque de 0x20 B al que apunta cam+0xE0.
 *   +0x00..+0x10  9 int16 Q12, w[3r+c] (fila 0 = x de pantalla,
 *                 fila 1 = y de pantalla, fila 2 = profundidad)
 *   +0x14/+0x18/+0x1C  3 int32 de traslacion
 * Es lo que consume 0x4269C2 (0x646BE0.. y 0x646BB4..). */
typedef struct {
    int16_t m[9];
    int32_t t[3];
} gex_cam_emit_matrix;

/* Construye la matriz de emision de la camara (0x46DD40, pasos 3 a 5 y
 * la traslacion de 0x46E4C6):
 *
 *   1. O = M2 * R (la misma O que se valida para la visibilidad);
 *   2. fila 0: (v << 9) / 0x140   (0x46E2A3..0x46E2E8);
 *   3. fila 1: (v * 0x180) / 0xF0  (0x46E2EC..0x46E33C);
 *   4. fila 2: sin escalar;
 *   5. cada fila escalada por cam+0x490/0x492/0x494 con imul + sar 12;
 *   6. traslacion = E * (-camPos) con sar 12 (0x46E4C6..0x46E51D).
 *
 * Devuelve 0 en exito y -1 con argumentos invalidos. */
int gex_cam_build_emit_matrix(const gex_camera *cam, gex_cam_emit_matrix *out);

/* Trigonometric tables (Q12). Filled by gex_cam_init_tables().
 * Full-turn tables, indexed by angle & 0xFFF; the internal lookup uses
 * quadrant identities on the sine table, as the original does.
 * gex_cam_cos_tab is exposed for reference/testing. */
extern int16_t gex_cam_sin_tab[0x1000];
extern int16_t gex_cam_cos_tab[0x1000];

/* Initialize sine/cosine tables in Q12 fixed point.
 * The original BSS tables at 0x00646C20 (sin) and 0x00645C20 (cos) are
 * filled at runtime; no initializer was located statically, so the port
 * generates equivalent mathematical values. */
void gex_cam_init_tables(void);

/* --- Rotation helpers --- */

/* Build rotation matrix about axis (row-major Q12) and pre-multiply:
 *   m = Rot(axis, angle) · m
 * angle: raw Q12 turns (0x1000 = one full turn), NOT pre-negated. */
void gex_cam_rot_z(gex_cam_mat3 *m, int16_t angle);
void gex_cam_rot_y(gex_cam_mat3 *m, int16_t angle);
void gex_cam_rot_x(gex_cam_mat3 *m, int16_t angle);

/* Multiply: dst = a · b  (both row-major Q12, intermediate sar 12). */
void gex_cam_mat_mul(gex_cam_mat3 *dst, const gex_cam_mat3 *a,
                     const gex_cam_mat3 *b);

/* Transpose a 3x3 matrix. */
void gex_cam_mat_transpose(gex_cam_mat3 *dst, const gex_cam_mat3 *src);

/* Change of axes: M2 = 4096 * [[1,0,0],[0,0,-1],[0,1,0]].
 * O = M2 · R (applied by columns). */
void gex_cam_apply_m2(gex_cam_mat3 *o, const gex_cam_mat3 *r);

/* --- Camera orientation chain --- */

/* build_orientation (0x46DD40):
 * R = RotX(x) · RotY(y) · RotZ(z) · base   (pre-multiplication, raw angles)
 * O = M2 · R
 * Writes O by columns into out_o. Returns 0 on success.
 * If flag_identity != 0 (or base == NULL, a port safety guard) the chain
 * starts from the identity matrix. */
int gex_cam_build_orientation(const gex_camera *cam, gex_cam_mat3 *out_o);

/* orientation_finalize (0x46E643):
 * M = transpose(O); translation handled separately (not needed for visibility).
 * Writes M into out_m. */
void gex_cam_orientation_finalize(const gex_cam_mat3 *o, gex_cam_mat3 *out_m);

/* build_local_frame (0x402DC0):
 * Builds local[0..4] from width and focal. Each corner cross product is
 * computed in int32, shifted with sar 12, truncated to int16 and then
 * approximately normalized. */
void gex_cam_build_local_frame(gex_camera *cam);

/* build_visibility_matrices (0x405C41):
 * A[row]      = M · local[row]   for row 0..2   (truncated to int16)
 * B[row 0..1] = M · local[row+3]               (truncated to int16)
 * B row 2 (+0xB4..0xB8) is preserved from the caller: the original reads
 * it but the writer was not located.
 * a_cam = A · camPos   (int32[3], no truncation)
 * b_cam = B · camPos   (int32[3], no truncation, includes B row 2) */
void gex_cam_build_visibility_matrices(gex_camera *cam);

/* Run the full orientation chain: local frame → M → A/B.
 * Convenience wrapper calling build_local_frame, build_orientation,
 * orientation_finalize, build_visibility_matrices in order. */
void gex_cam_update(gex_camera *cam);

/* --- Look-at del controlador de camara del menu ------------------- */

/* 0x40534F: longitud entera del vector (x, y, 0), con la aritmetica
 * exacta del original:
 *
 *   s   = x*x + y*y            (imul/add de 32 bits, con vuelta)
 *   s <<= 4
 *   r   = 0x4647EB(s, 4)       raiz entera con 4 bits fraccionarios
 *   len = (r + 8) >> 4         (add y sar 4)
 *
 * No usa sqrt() ni punto flotante. Devuelve el int32 que consume el
 * look-at como segundo argumento de 0x464181. */
int32_t gex_cam_length_2d(int16_t x, int16_t y);

/* 0x464181: atan2(y, x) en Q12 de media vuelta (2048 = pi), truncado
 * hacia cero. Casos especiales medidos en el original:
 *
 *   x == 0:  y >= 0 ? 0x400 : 0xFC00
 *   y == 0:  x > 0 ? 0 : 0x800
 *
 * En el resto reproduce la secuencia x87 del original:
 *   trunc(atan2(y, x) / 6.283185308 * 4096.0)
 * con la constante exacta de 0x54C178 y el truncamiento de 0x540950
 * (`fistpll` con la palabra de control a truncar). */
int16_t gex_cam_atan2_q12(int32_t y, int32_t x);

/* 0x406515 -> 0x40AE51: orientacion de la camara hacia `target`.
 *
 *   d0 = cam[0] - target[0]
 *   d1 = cam[1] - target[1]
 *   d2 = cam[2] - target[2]          (tres int16, con vuelta)
 *   len = 0x40534F(d0, d1)
 *   euler[0] = -0x464181(d2, len)
 *   euler[1] = 0
 *   euler[2] =  0x464181(d1, d0) + 0x400
 *
 * Los angulos quedan en Q12 de media vuelta (2048 = pi), que es el mismo
 * formato de `cam+0x30`. `euler` puede ser el propio `cam->euler`. */
void gex_cam_look_at(int16_t euler[3], const int16_t cam[3],
                     const int16_t target[3]);

/* --- Visibility test (0x00401C00) --- */

/* BSP node layout (verified): */
#define GEX_BSP_NODE_SIZE 32u
#define GEX_BSP_CENTER_X  0x00   /* int16 */
#define GEX_BSP_CENTER_Y  0x02   /* int16 */
#define GEX_BSP_CENTER_Z  0x04   /* int16 */
#define GEX_BSP_RADIUS    0x06   /* uint16, leído como unsigned */
#define GEX_BSP_NORMAL_X  0x0C   /* int16, Q12 */
#define GEX_BSP_NORMAL_Y  0x0E   /* int16, Q12 */
#define GEX_BSP_NORMAL_Z  0x10   /* int16, Q12 */
#define GEX_BSP_TYPE      0x12   /* int16: 1=interno, 2=hoja */
#define GEX_BSP_DISTANCE  0x14   /* int32 */
#define GEX_BSP_CHILD_LO  0x18   /* uint32 */
#define GEX_BSP_CHILD_HI  0x1C   /* uint32 */

/* Leaf predicate of the original executable: (type & 1) == 0.
 * The port uses this exactly; GEX_BSP_LEAF_FLAG (& 2) is only equivalent
 * for the observed types 1/2. */
static inline int gex_cam_node_is_leaf(uint16_t type) {
    return (type & 1u) == 0u;
}

/* Test a sphere (center c, unsigned radius r) against the six semiplanos
 * defined by camera matrices A/B and far_plane distance. Returns 1 if visible.
 * The original rejects when a distance is <= -r (or >= far_plane+r on the near
 * plane), i.e. it accepts:
 *
 *   (T_A.x − A_cam.x) > −r   &&  (T_A.x − A_cam.x) < far_plane + r
 *   (T_A.y − A_cam.y) > −r
 *   (T_A.z − A_cam.z) > −r
 *   (T_B.x − B_cam.x) > −r
 *   (T_B.y − B_cam.y) > −r
 *
 * with T_A = (A·c) >> 12, T_B = (B·c) >> 12, and A_cam/B_cam taken from
 * cam->a_cam / cam->b_cam. B row 2 is never compared. */
int gex_cam_sphere_visible(const gex_camera *cam,
                           const int16_t c[3], uint16_t radius);

/* Plane test for child ordering (0x00401C00 §7.4):
 * d = ((camPos · N) >> 12) − node_distance.
 *
 * Every step is 32-bit x86 register arithmetic: the three products are
 * summed modulo 2^32 (the sum can exceed INT32_MAX for large
 * coordinates), the shift is an arithmetic `sar 12` and the subtraction
 * wraps too. No signed overflow is involved. Returns d. */
int32_t gex_cam_plane_distance(const gex_camera *cam,
                               const int16_t normal[3], int32_t distance);

#ifdef __cplusplus
}
#endif

#endif /* GEX_CAMERA_H */
