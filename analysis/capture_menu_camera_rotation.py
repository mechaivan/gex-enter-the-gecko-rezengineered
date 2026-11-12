#!/usr/bin/env python3
"""Convierte una captura del trazador en el TSV de referencia de orientacion.

La captura se obtiene con el trazador del repo (tools/frame-trace) sobre una
copia propia del ejecutable original:

    tracer.exe GEX3D.EXE . rot_final2.log 240000 hw:406515 \
        1B,1B,1B,1B,0D,0D,28,0D,0D,20,0D,28,0D \
        dumpabs:65cd80:0,2,4,0x18,0x1a,0x1c,0x30,0x32,0x34 \
        dumpabs:5ac608:0,2,4 dumplimit:4000

El breakpoint 0x406515 es el calculo del "look-at" del controlador de camara
del menu (0x65CD80). En ese instante:

    0x65CD80 + 0x00/0x02/0x04   posicion de la camara
    0x5AC608 + 0x00/0x02/0x04   punto de mira (resultado de la trayectoria)
    0x65CD80 + 0x18/0x1A/0x1C   los tres angulos (el +0x18 es el valor del
                                frame anterior: se escribe justo despues)

Este script empareja cada estado capturado con el frame de la serie
publicada usando la posicion de la camara y escribe el TSV de referencia.
"""
import argparse
import io
import re
import sys

RE_POS = re.compile(r"\+0x0=0x([0-9A-Fa-f]+)\s+\+0x2=0x([0-9A-Fa-f]+)\s+\+0x4=0x([0-9A-Fa-f]+)")
RE_EUL = re.compile(r"\+0x18=0x([0-9A-Fa-f]+)\s+\+0x1A=0x([0-9A-Fa-f]+)\s+\+0x1C=0x([0-9A-Fa-f]+)")


def s16(v):
    v &= 0xFFFF
    return v - 0x10000 if v & 0x8000 else v


def words3(m):
    a, b, c = (int(g, 16) for g in m.groups())
    return (s16(a & 0xFFFF), s16((a >> 16) & 0xFFFF),
            s16(b & 0xFFFF), s16((b >> 16) & 0xFFFF),
            s16(c & 0xFFFF))


def read_capture(path):
    """Devuelve [(pos_cam, pos_mira, euler)] en orden de captura.

    En el instante del breakpoint el +0x18 todavia contiene los angulos del
    frame ANTERIOR (se escriben justo despues), asi que el angulo de un
    frame F es el +0x18 del impacto siguiente.
    """
    cam = {}
    mira = {}
    with io.open(path, encoding="utf-8", errors="replace") as fh:
        for line in fh:
            f = line.split("\t")
            if len(f) < 3 or f[0] != "ABS":
                continue
            seq = int(f[1])
            if "base=0x0065CD80" in line:
                v = words3(RE_POS.search(line))
                m = RE_EUL.search(line)
                cam[seq] = {"pos": (v[0], v[2], v[4]),
                             "eul": (s16(int(m.group(1), 16)),
                                     s16(int(m.group(2), 16)),
                                     s16(int(m.group(3), 16))) if m else None}
            elif "base=0x005AC608" in line:
                v = words3(RE_POS.search(line))
                mira[seq] = (v[0], v[2], v[4])
    rows = []
    for seq in sorted(set(cam) & set(mira)):
        rows.append((cam[seq]["pos"], mira[seq], cam[seq]["eul"]))
    return rows


def read_published(path):
    rows = []
    with io.open(path, encoding="utf-8") as fh:
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
    ap.add_argument("--log", required=True, help="captura del trazador")
    ap.add_argument("--tsv", default="analysis/menu_camera_map7.tsv")
    ap.add_argument("--out", default="analysis/menu_camera_rotation_map7.tsv")
    args = ap.parse_args()

    cap = read_capture(args.log)
    pub = read_published(args.tsv)
    print("estados capturados: %d ; frames publicados: %d" % (len(cap), len(pub)))

    by_euler = {}
    for want in pub:
        if want:
            by_euler.setdefault(want[3:6], []).append(want[0:3])

    matched = 0
    checked = 0
    bad = []
    for i, (pos, mir, eul) in enumerate(cap):
        if eul is None:
            continue
        # el +0x18 de este impacto son los angulos del frame anterior: se
        # comprueban contra la posicion del impacto anterior.
        if i == 0:
            continue
        prev_pos = cap[i - 1][0]
        checked += 1
        if eul in by_euler and prev_pos in by_euler[eul]:
            matched += 1
        elif len(bad) < 5:
            bad.append((i, prev_pos, eul))

    print("estados comprobados: %d ; angulos que coinciden con la serie "
          "publicada: %d" % (checked, matched))
    for b in bad:
        print("  impacto %d: posicion anterior %s, angulos %s" % b)
    return 0 if matched == checked else 1


if __name__ == "__main__":
    sys.exit(main())
