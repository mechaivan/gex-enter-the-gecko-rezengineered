#!/usr/bin/env python3
"""Referencia de la ORIENTACION de la camara del menu (MAP7).

Genera analysis/menu_camera_rotation_map7.tsv con los 428 estados de
orientacion que el ejecutable original produce durante el vuelo, calculados
desde LEVEL\\MAP7.DFX con la aritmetica **entera** medida del original
(GEX3D.EXE, build europeo).

Que reproduce esta referencia
----------------------------

El controlador de camara del menu (0x65CD80) tiene DOS trayectorias, ambas
en el DFX de MAP7 con el mismo formato de registro de 32 B:

    array de POSICION  imagen 0xC034   26 registros   (actor 0x95EC, slot 0)
    array de MIRA     imagen 0xC39C   26 registros   (actor 0x961C, slot 1)

Cada registro es  D (int16) | punto (3 x int16) | tangente out (3 x int32) |
tangente in (3 x int32),  igual que la trayectoria de posicion ya integrada.

La evaluacion (0x53E98C -> 0x53F252 + 0x53EF00 -> 0x53DEB0) es la Hermite
cubica en Q15:

    t      = (frac << 15) / D          (frac es la fraccion de 15 bits)
    t2     = (t * t) >> 15
    t3     = (t2 * t) >> 15
    h00    = 2t^3 - 3t^2 + 32768
    h01    = 3t^2 - 2t^3
    h10    = t^3 - 2t^2 + t
    h11    = t^3 - t^2
    punto  = (p0*h00 + p1*h01 + tout0*h10 + tin1*h11) >> 15   (truncado a int16)

Los tres angulos NO vienen de un cuaternion: los calcula 0x406515 ->
0x40AE51 como un "look-at" desde la posicion de la camara hacia el punto de
mira:

    d0 = cam.x - mira.x
    d1 = cam.y - mira.y
    d2 = cam.z - mira.z
    len  = (4*isqrt((d0^2 + d1^2) << 4) + 8) >> 4        (0x40534F)
    euler_x = -trunc(atan2(d2, len) * 2048/pi)            (0x464181)
    euler_y = 0
    euler_z =  trunc(atan2(d1, d0) * 2048/pi) + 0x400

Los angulos estan en Q12 de media vuelta (2048 = pi) y se copian de
cam+0x18 a cam+0x30 (0x4122E4), que es lo que consume el renderer.

Verificacion
------------

El script comprueba al final que los angulos calculados coinciden con los
de la serie publicada analysis/menu_camera_map7.tsv (428/428) y con la
captura directa del ejecutable original.

Uso:

    py analysis/gen_menu_camera_rotation_ref.py --dfx <MAP7.DFX> \\
        [--tsv analysis/menu_camera_map7.tsv] \\
        [--out analysis/menu_camera_rotation_map7.tsv]
"""

import argparse
import math
import struct
import sys

# Offsets medidos en la imagen compactada de LEVEL\MAP7.DFX.
ACTOR_SLOT0 = 0x95EC        # header 0x5FC -> 4F/0: actor de la POSICION
ACTOR_SLOT1 = 0x961C        # header 0x5FC -> 4F/1: actor de la MIRA
HEADER_PAIR = 0x5FC
KEY_SIZE = 32
REC_COUNT = 26

S = 15                     # Q15 de la Hermite
HALF_TURN = 2048.0 / math.pi   # Q12 de media vuelta: 2048 = pi


def fail(msg):
    sys.stderr.write("ERROR: %s\n" % msg)
    raise SystemExit(1)


def u16(buf, off):
    return struct.unpack_from("<H", buf, off)[0]


def u32(buf, off):
    return struct.unpack_from("<I", buf, off)[0]


def s16(buf, off):
    return struct.unpack_from("<h", buf, off)[0]


def s32(buf, off):
    return struct.unpack_from("<i", buf, off)[0]


def load_image(dfx_path):
    data = open(dfx_path, "rb").read()
    count = u32(data, 0)
    aligned = (count + 0x200) & ~0x1FF
    image_offset = aligned * 4
    if image_offset >= len(data):
        fail("image_offset fuera del fichero")
    return data[image_offset:]


def follow_chain(image, actor_slot):
    """Cadena de punteros: header -> actor -> entidad -> objeto -> array."""
    want = (0x4F, 0, ACTOR_SLOT0, 0x4F, 1, ACTOR_SLOT1, 0x0E)
    got = tuple(u32(image, HEADER_PAIR + i * 4) for i in range(7))
    if got != want:
        fail("el header de nivel en 0x%X no es la pareja esperada: %r"
             % (HEADER_PAIR, got))

    entity = u32(image, actor_slot + 0x28)
    obj = u32(image, entity + 0x00)
    keys = u32(image, obj + 0x00)
    count = u16(image, obj + 0x04)
    variant = image[obj + 6]
    flags = image[obj + 7]
    if variant != 0:
        fail("variante %d, se esperaba 0 (registros de 32 B)" % variant)
    if count != REC_COUNT:
        fail("count %d, se esperaban %d" % (count, REC_COUNT))
    if keys + count * KEY_SIZE > len(image):
        fail("los registros se salen de la imagen")
    return entity, obj, keys, count, flags


def read_records(image, keys, count):
    recs = []
    for i in range(count):
        o = keys + i * KEY_SIZE
        recs.append((
            s16(image, o + 0x00),
            (s16(image, o + 0x02), s16(image, o + 0x04), s16(image, o + 0x06)),
            (s32(image, o + 0x08), s32(image, o + 0x0C), s32(image, o + 0x10)),
            (s32(image, o + 0x14), s32(image, o + 0x18), s32(image, o + 0x1C)),
        ))
    return recs


def hermite(a, b, t):
    """0x53DEB0: Hermite cubica en Q15 con enteros."""
    t2 = (t * t) >> S
    t3 = (t2 * t) >> S
    h00 = 2 * t3 - 3 * t2 + (1 << S)
    h01 = 3 * t2 - 2 * t3
    h10 = t3 - 2 * t2 + t
    h11 = t3 - t2
    out = []
    for k in range(3):
        v = (a[1][k] * h00 + b[1][k] * h01 +
             a[2][k] * h10 + b[3][k] * h11)
        v = (v >> S) & 0xFFFF
        out.append(v - 0x10000 if v & 0x8000 else v)
    return tuple(out)


def advance(idx, frac, recs):
    """0x53F252: avanza el nodo. Devuelve (indice, fraccion, sigue)."""
    limit = recs[idx][0]
    frac = (frac + 1) & 0x7FFF
    if frac < limit:
        return idx, frac, 1
    idx += 1
    frac = 0
    if idx < len(recs) - 1:
        return idx, frac, 1
    return idx, frac, (1 if idx <= len(recs) - 1 else 0)


def sample(recs, idx, frac):
    """0x53EF00: copia directa si D == 0 o frac == 0; si no, Hermite."""
    d = recs[idx][0]
    if d == 0 or frac == 0:
        return tuple(recs[idx][1])
    t = (frac << S) // d
    return hermite(recs[idx], recs[idx + 1], t)


def isqrt(n):
    x, y = n, (n + 1) >> 1
    while y < x:
        x = y
        y = (x + n // x) >> 1
    return x


def atan2_q12(y, x):
    """0x464181 + 0x540950: Q12 de media vuelta, truncado hacia cero."""
    if x == 0:
        return 1024 if y >= 0 else -1024
    if y == 0:
        return 0 if x > 0 else 2048
    return int(math.atan2(float(y), float(x)) * HALF_TURN)


def look_at(cam, mira):
    """0x40AE51: los tres angulos del controlador de camara."""
    d0 = cam[0] - mira[0]
    d1 = cam[1] - mira[1]
    d2 = cam[2] - mira[2]
    ln = (4 * isqrt((d0 * d0 + d1 * d1) << 4) + 8) >> 4
    return (-atan2_q12(d2, ln), 0, atan2_q12(d1, d0) + 0x400)


def read_published(path):
    rows = []
    with open(path, encoding="utf-8") as fh:
        for line in fh:
            if line.startswith("#") or line.startswith("frame"):
                continue
            p = line.rstrip("\n").split("\t")
            if len(p) < 8 or p[2] == "":
                rows.append(None)
                continue
            rows.append((int(p[2]), int(p[3]), int(p[4]),
                         int(p[5]), int(p[6]), int(p[7])))
    return rows


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--dfx", required=True)
    ap.add_argument("--tsv", default="analysis/menu_camera_map7.tsv")
    ap.add_argument("--out", default="analysis/menu_camera_rotation_map7.tsv")
    args = ap.parse_args()

    image = load_image(args.dfx)
    ent_p, obj_p, keys_p, count_p, flags_p = follow_chain(image, ACTOR_SLOT0)
    ent_m, obj_m, keys_m, count_m, flags_m = follow_chain(image, ACTOR_SLOT1)
    print("POSICION: actor 0x%04X -> entidad 0x%04X -> objeto 0x%04X -> "
          "array 0x%04X (%d registros, flags %d)"
          % (ACTOR_SLOT0, ent_p, obj_p, keys_p, count_p, flags_p))
    print("MIRA   : actor 0x%04X -> entidad 0x%04X -> objeto 0x%04X -> "
          "array 0x%04X (%d registros, flags %d)"
          % (ACTOR_SLOT1, ent_m, obj_m, keys_m, count_m, flags_m))

    pos_recs = read_records(image, keys_p, count_p)
    mira_recs = read_records(image, keys_m, count_m)

    pub = read_published(args.tsv)
    frames = len(pub)
    print("frames de la serie publicada: %d" % frames)

    idx_p = frac_p = 0
    idx_m = frac_m = 0
    rows = []
    ok = bad = 0
    for frame in range(frames):
        idx_p, frac_p, ret_p = advance(idx_p, frac_p, pos_recs)
        idx_m, frac_m, ret_m = advance(idx_m, frac_m, mira_recs)
        if not ret_p or not ret_m:
            break
        p_pos = sample(pos_recs, idx_p, frac_p)
        p_mira = sample(mira_recs, idx_m, frac_m)
        euler = look_at(p_pos, p_mira)
        rows.append((frame, p_pos, p_mira, euler))
        want = pub[frame]
        if want is None:
            continue
        if (p_pos == want[0:3]) and (euler == want[3:6]):
            ok += 1
        else:
            bad += 1
            if bad <= 5:
                print("  frame %3d: pos %s (publicado %s)  euler %s "
                      "(publicado %s)"
                      % (frame, p_pos, want[0:3], euler, want[3:6]))

    print("coincidencias exactas con la serie publicada: %d/%d" % (ok, ok + bad))
    if bad:
        fail("la referencia calculada no reproduce la serie publicada")

    with open(args.out, "w", encoding="utf-8", newline="\n") as fh:
        fh.write("# Orientacion de la camara del menu (Gex: Enter the Gecko, EU)\n")
        fh.write("# sobre MAP7 durante el vuelo de 428 frames.\n")
        fh.write("#\n")
        fh.write("# GENERADO por analysis/gen_menu_camera_rotation_ref.py desde\n")
        fh.write("# LEVEL\\MAP7.DFX con la aritmetica entera del original:\n")
        fh.write("#   0x53E98C -> 0x53F252 (avance) + 0x53EF00 -> 0x53DEB0 (Hermite Q15)\n")
        fh.write("#   0x406515 -> 0x40AE51 (look-at) con 0x40534F (longitud) y\n")
        fh.write("#   0x464181 (atan2 en Q12 de media vuelta, truncado).\n")
        fh.write("#\n")
        fh.write("# cam  = posicion de la camara (array 0xC034 del DFX)\n")
        fh.write("# mira = punto de mira (array 0xC39C del DFX)\n")
        fh.write("# Los angulos estan en Q12 de media vuelta (2048 = pi).\n")
        fh.write("#\n")
        fh.write("# frame\tcam_x\tcam_y\tcam_z\tmira_x\tmira_y\tmira_z"
                 "\teuler_x\teuler_y\teuler_z\n")
        for frame, p_pos, p_mira, euler in rows:
            fh.write("%d\t%d\t%d\t%d\t%d\t%d\t%d\t%d\t%d\t%d\n" %
                     (frame, p_pos[0], p_pos[1], p_pos[2],
                      p_mira[0], p_mira[1], p_mira[2],
                      euler[0], euler[1], euler[2]))
    print("escrito %s (%d frames)" % (args.out, len(rows)))
    return 0


if __name__ == "__main__":
    sys.exit(main())
