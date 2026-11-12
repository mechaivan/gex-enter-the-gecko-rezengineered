#include "gex/camera_path.h"

#include <string.h>

/*
 * The original works on 32-bit registers: products and sums wrap, `sar`
 * is arithmetic, `idiv` truncates toward zero and the final store is a
 * 16-bit truncation. The helpers below reproduce each of those steps
 * without relying on implementation-defined C behaviour.
 */

static int32_t wrap32(uint32_t value) {
    int32_t out;
    memcpy(&out, &value, sizeof out);
    return out;
}

static int32_t mul32(int32_t a, int32_t b) {
    return wrap32((uint32_t)a * (uint32_t)b);
}

static int32_t sar32(int32_t value, unsigned bits) {
    if (value >= 0) return (int32_t)((uint32_t)value >> bits);
    return -1 - (int32_t)((uint32_t)(~value) >> bits);
}

static int16_t trunc16(int32_t value) {
    uint16_t low = (uint16_t)((uint32_t)value & 0xFFFFu);
    int16_t out;
    memcpy(&out, &low, sizeof out);
    return out;
}

void gex_camera_path_init_node(gex_camera_path_node *node) {
    if (node == NULL) return;
    node->index = 0;
    node->fraction = 0;
}

uint16_t gex_camera_path_fraction(const gex_camera_path_node *node) {
    if (node == NULL) return 0;
    return (uint16_t)((node->fraction >> 1) & GEX_CAMERA_PATH_FRAC_MASK);
}

uint16_t gex_camera_path_mode(const gex_camera_path_node *node) {
    if (node == NULL) return 0;
    return (uint16_t)(node->fraction & 1u);
}

int gex_camera_path_advance(const gex_camera_path *path,
                            gex_camera_path_node *node) {
    int32_t limit;
    int32_t fraction;
    int16_t index;
    int ret = 1;

    if (path == NULL || node == NULL || path->keys == NULL) return 0;
    if (path->variant != GEX_CAMERA_PATH_VARIANT_POSITION) return 0;
    if (path->count <= 0) return 0;

    index = node->index;
    if (index < 0 || index >= path->count) return 0;

    limit = path->keys[index].duration;

    /* 0x53E0CB(obj, node, 0): with mode bit 0 clear it returns at once.
     * With the bit set it rescales the stored fraction by 2^15 / D. */
    if ((node->fraction & 1u) != 0u) {
        fraction = (int32_t)gex_camera_path_fraction(node);
        if (fraction != 0) {
            uint32_t duration = (uint32_t)(limit != 0 ? limit : 1);
            uint32_t scaled =
                ((uint32_t)fraction << GEX_CAMERA_PATH_FRAC_BITS) / duration;
            fraction = (int32_t)(scaled & GEX_CAMERA_PATH_FRAC_MASK);
            node->fraction = (uint16_t)((fraction << 1) | (node->fraction & 1u));
        }
    }

    /* frac = ((frac + 1) & 0x7FFF); bit 0 is preserved. */
    fraction = (int32_t)gex_camera_path_fraction(node);
    fraction = (fraction + 1) & GEX_CAMERA_PATH_FRAC_MASK;
    node->fraction = (uint16_t)((fraction << 1) | (node->fraction & 1u));
    if (fraction < limit) return 1;

    /* Segment finished: index++, fraction = 0 (bit 0 preserved). */
    node->index = (int16_t)(index + 1);
    node->fraction = (uint16_t)(node->fraction & 1u);
    index = node->index;
    if (index < path->count - 1) return 1;
    if (index > path->count - 1) ret = 0;
    if ((path->flags & (GEX_CAMERA_PATH_FLAG_WRAP_A |
                        GEX_CAMERA_PATH_FLAG_WRAP_B)) != 0u) {
        node->index = 0;
        return ret;
    }
    node->index = (int16_t)(path->count - 1);
    return ret;
}

/* 0x53DEB0: Hermite in Q15, integer only. */
static void hermite_sample(int16_t out[3], int32_t t,
                           const gex_camera_path_key *a,
                           const gex_camera_path_key *b) {
    int32_t t2, t3, h00, h01, h10, h11;
    int axis;

    if (t == 0) {
        out[0] = a->point[0];
        out[1] = a->point[1];
        out[2] = a->point[2];
        return;
    }

    t2 = sar32(mul32(t, t), GEX_CAMERA_PATH_SHIFT);
    t3 = sar32(mul32(t2, t), GEX_CAMERA_PATH_SHIFT);

    /* h00 = 2t^3 - 3t^2 + 32768 */
    h00 = wrap32(wrap32((uint32_t)t3 << 1) - (uint32_t)mul32(t2, 3) +
                 (uint32_t)GEX_CAMERA_PATH_ONE);
    /* h01 = 3t^2 - 2t^3 */
    h01 = wrap32((uint32_t)mul32(t2, 3) - wrap32((uint32_t)t3 << 1));
    /* h10 = t^3 - 2t^2 + t */
    h10 = wrap32(wrap32((uint32_t)t3 - (uint32_t)mul32(t2, 2)) + (uint32_t)t);
    /* h11 = t^3 - t^2 */
    h11 = wrap32((uint32_t)t3 - (uint32_t)t2);

    for (axis = 0; axis < 3; axis++) {
        uint32_t acc = (uint32_t)mul32((int32_t)a->point[axis], h00);
        acc += (uint32_t)mul32((int32_t)b->point[axis], h01);
        acc += (uint32_t)mul32(a->tangent_out[axis], h10);
        acc += (uint32_t)mul32(b->tangent_in[axis], h11);
        out[axis] = trunc16(sar32(wrap32(acc), GEX_CAMERA_PATH_SHIFT));
    }
}

int gex_camera_path_eval(const gex_camera_path *path,
                         const gex_camera_path_node *node,
                         int16_t out[3]) {
    const gex_camera_path_key *a;
    int32_t limit = GEX_CAMERA_PATH_ONE; /* 0x8000 when the mode bit is set */
    int32_t fraction;
    int32_t t;

    if (path == NULL || node == NULL || out == NULL) return 0;
    if (path->keys == NULL) return 0;
    if (path->variant != GEX_CAMERA_PATH_VARIANT_POSITION) return 0;
    if (path->count <= 0) return 0;
    if (node->index < 0 || node->index >= path->count) return 0;

    if ((node->fraction & 1u) == 0u) limit = path->keys[node->index].duration;
    fraction = (int32_t)gex_camera_path_fraction(node);

    a = &path->keys[node->index];
    if (limit == 0 || fraction == 0) {
        out[0] = a->point[0];
        out[1] = a->point[1];
        out[2] = a->point[2];
        return 1;
    }

    /* The original reads key[index + 1]. The sentinel (last key, D == 0)
     * is handled above, so a positive D on the last key is malformed and
     * is rejected instead of reading past the array. */
    if (node->index + 1 >= path->count) return 0;

    /* t = (frac << 15) / D, signed division truncated toward zero. */
    t = (int32_t)((uint32_t)fraction << GEX_CAMERA_PATH_SHIFT) / limit;
    hermite_sample(out, t, a, a + 1);
    return 1;
}

int gex_camera_path_step(const gex_camera_path *path,
                         gex_camera_path_node *node,
                         int16_t position[3]) {
    int16_t sample[3];

    if (path == NULL || node == NULL || position == NULL) return 0;
    if (gex_camera_path_advance(path, node) == 0) return 0;
    if (gex_camera_path_eval(path, node, sample) == 0) return 0;
    position[0] = sample[0];
    position[1] = sample[1];
    position[2] = sample[2];
    return 1;
}

int gex_camera_path_validate(const gex_camera_path *path) {
    int16_t i;

    if (path == NULL || path->keys == NULL) return 0;
    if (path->count < 2) return 0;
    if (path->variant != GEX_CAMERA_PATH_VARIANT_POSITION) return 0;
    for (i = 0; i < path->count - 1; i++) {
        if (path->keys[i].duration < 0) return 0;
    }
    if (path->keys[path->count - 1].duration != 0) return 0;
    return 1;
}
