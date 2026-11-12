#!/usr/bin/env python3
"""Verifica el interpolador de la camara del menu contra las 428 muestras.

Replica la logica medida en el ejecutable original, no una aproximacion:

    0x53F252  avance del nodo      (+1 en la fraccion de 15 bits por frame)
    0x53EF00  seleccion            (copia directa si D == 0 o frac == 0)
    0x53DEB0  interpolacion        (Hermite cubica en Q15, enteros)

Datos medidos que usa:

  - array de 26 registros de 32 B incrustado en el objeto (`obj[+0]` -> `obj+8`)
  - S = [0x0054C1B0] = 15
  - +1 en la fraccion de 15 bits por llamada; al llegar a D, `idx++` y `frac = 0`
  - recorte solo en el ultimo indice; el registro 25 es un centinela con D = 0
  - si el avance devuelve 0 no se evalua: la camara conserva el valor anterior

Uso:

    python verify_menu_camera_interp.py <trace_arr.log> [--ptr 0x193E6F80]
                                        [--tsv analysis/menu_camera_map7.tsv]

El log se genera con el trazador del repo (`tools/frame-trace`), con un
breakpoint en `0x53EF00` y `dumpindraw:0:0:840` (el array va en `obj[+0]`):

    tracer.exe GEX3D.EXE . trace_arr.log 30000 hw:53ef00 none \
        dumpindraw:0:0:840 dumplimit:2

El puntero del array cambia entre ejecuciones: se lee de la linea `INDRAW`
del propio log (`ptr=...`). Este script no contiene datos del juego.
"""
import argparse
import io
import struct
import sys

S = 15        # [0x0054C1B0], medido en runtime
NREC = 26     # obj[+4]: numero de registros (indices 0..25)
LAST = NREC - 1
NSEG = 25     # segmentos reales = registros 0..24


def load_array(log_path, ptr):
    with io.open(log_path, encoding="utf-8", errors="replace") as f:
        for line in f:
            if not line.startswith("INDRAW"):
                continue
            p = line.rstrip("\n").split("\t")
            got = int(p[3].split("=")[1], 16)
            if ptr is not None and got != ptr:
                continue
            return got, bytes(int(x, 16) for x in p[5].split())
    raise SystemExit("no hay ninguna linea INDRAW con ptr=0x%08X" % (ptr or 0))


def parse_records(b):
    recs = []
    for i in range(NREC + 1):        # +1: permite leer b = a + 32
        o = i * 32
        recs.append((struct.unpack_from("<h", b, o)[0],
                     struct.unpack_from("<3h", b, o + 2),
                     struct.unpack_from("<3i", b, o + 8),
                     struct.unpack_from("<3i", b, o + 0x14)))
    return recs


def s16(v):
    v &= 0xFFFF
    return v - 0x10000 if v & 0x8000 else v


def interp(t, a, b):
    """0x53DEB0(out, t, a, b): Hermite cubica en Q15 con enteros."""
    t2 = (t * t) >> S
    t3 = (t2 * t) >> S
    h00 = 2 * t3 - 3 * t2 + (1 << S)
    h01 = 3 * t2 - 2 * t3
    h10 = t3 - 2 * t2 + t
    h11 = t3 - t2
    out = []
    for k in range(3):
        v = a[1][k] * h00 + b[1][k] * h01 + a[2][k] * h10 + b[3][k] * h11
        out.append(s16(v >> S))
    return tuple(out)


def evaluate(idx, frac, recs):
    """0x53EF00: copia directa si D == 0 o frac == 0; si no, Hermite."""
    d = recs[idx][0]
    if d == 0 or frac == 0:
        return recs[idx][1]
    return interp((frac << S) // d, recs[idx], recs[idx + 1])


def advance(idx, frac, recs):
    """0x53F252: devuelve (idx, frac, ret). ret = 0 -> no se evalua."""
    if idx >= NREC:
        return idx, frac, 0
    d = recs[idx][0]
    frac = (frac + 1) & 0x7FFF
    if frac < d:
        return idx, frac, 1
    idx += 1
    frac = 0
    if idx < LAST:
        return idx, frac, 1
    ret = 1 if idx <= LAST else 0
    idx = LAST                     # recorte (obj[7] & 2 / & 4 no puestos)
    return idx, frac, ret


def read_tsv(path):
    rows = []
    with io.open(path, encoding="utf-8", errors="replace") as f:
        for line in f:
            if line.startswith("#") or not line.strip():
                continue
            p = line.split("\t")
            rows.append((int(p[0]), int(p[2]), int(p[3]), int(p[4])))
    return rows


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("log")
    ap.add_argument("--ptr", default=None, help="direccion del array (hex); por defecto, la primera INDRAW")
    ap.add_argument("--tsv", default="analysis/menu_camera_map7.tsv")
    ap.add_argument("--frames", type=int, default=428)
    a = ap.parse_args()

    ptr = int(a.ptr, 16) if a.ptr else None
    ptr, blob = load_array(a.log, ptr)
    recs = parse_records(blob)
    print("array 0x%08X | %d registros de 32 B" % (ptr, NREC))

    durs = [recs[i][0] for i in range(NSEG)]
    print("D de los 25 segmentos:", durs)
    print("suma:", sum(durs), "| D del centinela (rec 25):", recs[LAST][0])

    idx = frac = 0
    traj = []
    for frame in range(a.frames):
        idx, frac, ret = advance(idx, frac, recs)
        traj.append((frame, idx, frac, evaluate(idx, frac, recs) if ret else None))

    pub = [r for r in read_tsv(a.tsv) if r[0] < a.frames]
    print("muestras en el TSV:", len(pub))

    ok, bad = 0, []
    for (fr, px, py, pz), (_, i2, f2, out) in zip(pub, traj):
        if out == (px, py, pz):
            ok += 1
        else:
            bad.append((fr, i2, f2, out, (px, py, pz)))
    print("coincidencias exactas: %d/%d" % (ok, len(pub)))
    for fr, i2, f2, out, got in bad[:20]:
        print("  f=%3d idx=%2d frac=%2d calc=%s tsv=%s" % (fr, i2, f2, out, got))
    if bad:
        return 1
    print("OK: la formula reproduce las muestras sin tolerancia")
    return 0


if __name__ == "__main__":
    sys.exit(main())
