#!/usr/bin/env python3
"""Genera port/tests/ref_menu_camera_path.h desde MAP7.DFX y el TSV medido.

Sigue la cadena real del contenedor (offsets de imagen compactada, no un
escaneo de bytes):

    header de nivel, imagen 0x5FC : 4F 00 00 00 00 -> 0x95EC  (slot 0 del actor)
    0x95EC + 0x28 -> entidad        (0xC00C)
    entidad + 0x00 -> objeto        (0xC02C)
    objeto + 0x00 -> 26 registros   (0xC034), +0x04 = count, +0x06 = variante,
                                    +0x07 = flags

El script comprueba cada paso (la pareja 0x4F del header, la cabecera del
objeto) y aborta si algo no cuadra. No incrusta datos del juego: los lee
del DFX que se le pasa.

Uso:

    py analysis/gen_menu_camera_path_ref.py --dfx <MAP7.DFX> \
        [--tsv analysis/menu_camera_map7.tsv] \
        [--out port/tests/ref_menu_camera_path.h]
"""

import argparse
import struct
import sys

ACTOR_SLOT0 = 0x95EC          # imagen: registro de actor, slot 0 (posicion)
ACTOR_SLOT1 = 0x961C          # imagen: registro de actor, slot 1 (hermano)
HEADER_PAIR = 0x5FC           # imagen: "4F 0 <0x95EC> 4F 1 <0x961C> 0E"
KEY_SIZE = 32


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


def follow_chain(image):
    # El header del nivel referencia la pareja (slot 0, slot 1) del actor.
    want = (0x4F, 0, ACTOR_SLOT0, 0x4F, 1, ACTOR_SLOT1, 0x0E)
    got = (
        u32(image, HEADER_PAIR),
        u32(image, HEADER_PAIR + 4),
        u32(image, HEADER_PAIR + 8),
        u32(image, HEADER_PAIR + 12),
        u32(image, HEADER_PAIR + 16),
        u32(image, HEADER_PAIR + 20),
        u32(image, HEADER_PAIR + 24),
    )
    if got != want:
        fail("el header de nivel en 0x%X no es la pareja esperada: %r" %
             (HEADER_PAIR, got))

    entity = u32(image, ACTOR_SLOT0 + 0x28)
    obj = u32(image, entity + 0x00)
    keys = u32(image, obj + 0x00)
    count = u16(image, obj + 0x04)
    variant = image[obj + 0x06]
    flags = image[obj + 0x07]
    if variant != 0:
        fail("variante %d, se esperaba 0 (registros de 32 B)" % variant)
    if keys + count * KEY_SIZE > len(image):
        fail("los registros se salen de la imagen")

    records = []
    for i in range(count):
        off = keys + i * KEY_SIZE
        records.append({
            "duration": s16(image, off + 0x00),
            "point": (s16(image, off + 0x02),
                      s16(image, off + 0x04),
                      s16(image, off + 0x06)),
            "tangent_out": (s32(image, off + 0x08),
                            s32(image, off + 0x0C),
                            s32(image, off + 0x10)),
            "tangent_in": (s32(image, off + 0x14),
                           s32(image, off + 0x18),
                           s32(image, off + 0x1C)),
        })
    return entity, obj, keys, count, variant, flags, records


def load_samples(tsv_path):
    samples = []
    for line in open(tsv_path, "r", encoding="utf-8", errors="replace"):
        line = line.strip()
        if not line or line.startswith("#"):
            continue
        fields = line.split("\t")
        if len(fields) < 5:
            continue
        samples.append((int(fields[2]), int(fields[3]), int(fields[4])))
    return samples


def emit(records, samples, entity, obj, keys, flags, out_path):
    total = sum(r["duration"] for r in records[:-1])
    lines = []
    lines.append("/*")
    lines.append(" * ref_menu_camera_path.h - referencia de la trayectoria de la camara")
    lines.append(" * del menu sobre MAP7 (S-3).")
    lines.append(" *")
    lines.append(" * GENERADO por analysis/gen_menu_camera_path_ref.py. No editar a mano.")
    lines.append(" *")
    lines.append(" * Origen, cadena seguida en la imagen compactada de LEVEL\\MAP7.DFX:")
    lines.append(" *   header de nivel 0x5FC  -> 4F/0 0x%04X y 4F/1 0x%04X" %
                 (ACTOR_SLOT0, ACTOR_SLOT1))
    lines.append(" *   actor 0x%04X + 0x28   -> entidad 0x%04X" % (ACTOR_SLOT0, entity))
    lines.append(" *   entidad + 0x00        -> objeto 0x%04X" % obj)
    lines.append(" *   objeto + 0x00         -> %d registros de 32 B en 0x%04X" %
                 (len(records), keys))
    lines.append(" *   objeto + 0x04 = %d, +0x06 = 0 (variante), +0x07 = %d (flags)" %
                 (len(records), flags))
    lines.append(" *")
    lines.append(" * Los registros son byte a byte los del array que el original tiene")
    lines.append(" * en memoria durante el vuelo (capturado en 0x193E6F80: 832 B")
    lines.append(" * identicos). Las muestras son las 428 posiciones medidas del")
    lines.append(" * update por frame del original (analysis/menu_camera_map7.tsv).")
    lines.append(" *")
    lines.append(" * Duraciones de los %d segmentos: %s = %d" %
                 (len(records) - 1, [r["duration"] for r in records[:-1]], total))
    lines.append(" * Primera muestra: %s" % (samples[0],))
    lines.append(" * Ultima muestra:  %s" % (samples[-1],))
    lines.append(" */")
    lines.append("#ifndef GEX_REF_MENU_CAMERA_PATH_H")
    lines.append("#define GEX_REF_MENU_CAMERA_PATH_H")
    lines.append("")
    lines.append('#include "gex/camera_path.h"')
    lines.append("")
    lines.append("#define GEX_REF_MENU_CAMERA_PATH_COUNT %d" % len(records))
    lines.append("#define GEX_REF_MENU_CAMERA_PATH_FRAMES %d" % len(samples))
    lines.append("#define GEX_REF_MENU_CAMERA_PATH_DURATION_TOTAL %d" % total)
    lines.append("")
    lines.append("static const gex_camera_path_key gex_ref_menu_camera_path[] = {")
    for i, r in enumerate(records):
        lines.append("    { %d, { %d, %d, %d }, { %d, %d, %d }, { %d, %d, %d } },"
                     " /* %d */" % (
                         r["duration"], *r["point"], *r["tangent_out"],
                         *r["tangent_in"], i))
    lines.append("};")
    lines.append("")
    lines.append("static const int16_t gex_ref_menu_camera_position[%d][3] = {" %
                 len(samples))
    for i, s in enumerate(samples):
        lines.append("    { %d, %d, %d }, /* %d */" % (s[0], s[1], s[2], i))
    lines.append("};")
    lines.append("")
    lines.append("#endif /* GEX_REF_MENU_CAMERA_PATH_H */")
    lines.append("")
    open(out_path, "w", encoding="utf-8", newline="\n").write("\n".join(lines))
    print("escrito %s: %d registros, %d muestras, %d frames" %
          (out_path, len(records), len(samples), total))


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--dfx", required=True)
    ap.add_argument("--tsv", default="analysis/menu_camera_map7.tsv")
    ap.add_argument("--out", default="port/tests/ref_menu_camera_path.h")
    args = ap.parse_args()

    image = load_image(args.dfx)
    entity, obj, keys, count, variant, flags, records = follow_chain(image)
    samples = load_samples(args.tsv)
    if count != 26:
        fail("count %d, se esperaban 26" % count)
    if len(samples) != 428:
        fail("%d muestras, se esperaban 428" % len(samples))
    total = sum(r["duration"] for r in records[:-1])
    if total != 428:
        fail("suma de duraciones %d, se esperaba 428" % total)
    if records[-1]["duration"] != 0:
        fail("el ultimo registro no es el centinela")
    emit(records, samples, entity, obj, keys, flags, args.out)


if __name__ == "__main__":
    main()
