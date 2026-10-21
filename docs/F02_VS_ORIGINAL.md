# Comparativa F-02 (exe 2013 NTSC-D3D) vs Original EU — Gex: Enter the Gecko (PC)

> **Nivel de evidencia:** mixto. Lo marcado **CONFIRMADO** proviene de evidencia
> directa (inventario, hashes, strings). Lo marcado **HIPÓTESIS** es inferencia
> explícita. Lo marcado **UNKNOWN** está pendiente de verificación.
> Fuentes: `PATCH_ANALYSIS.md` (F-02, F-04, F-07, F-09, F-11),
> `KNOWN_ISSUES.md`, `REVERSE_ENGINEERING.md` (Etapa C/D), `RESEARCH.md`
> (S-04, S-14), `MODERNIZATION_GOALS.md` (FA-01, FA-03, FA-12),
> `docs/ORIGINAL_ARTIFACT_INVENTORY.md`.

## 1. Identidad de los binarios

| Campo | Original EU (Glide) | F-02 / exe 2013 NTSC-D3D |
|---|---|---|
| Fichero | `GEX3D.EXE` | `gex3d.exe` (2013, "parcheado") |
| md5 | `692B1282…` | Pendiente de verificar (inventario S-14) |
| sha256 | `692b1282…` (pendiente) | Pendiente de verificar |
| TimeDateStamp (PE) | `0x3560A32E` (19-may-1998) | Desconocido (2013) |
| Tamaño | ~1.1 MB | Desconocido |
| Versión declarada | 1.00.000 | Desconocida |
| Renderer | Glide (3DFX) | Direct3D (D3D) |
| Región | EU (voces UK, `voiceuk`) | NTSC (voces US, `voice`) |
| Procedencia | CD original (CloneCD) | Mysticore speedrun.com (S-14, Drive) |

**CONFIRMADO:** son **binarios de orígenes distintos** (EU-Glide vs NTSC-D3D,
diferente renderer, diferente región, diferente fecha). No son intercambiables.

## 2. Relación con otros fixes

| Fix | Relación con F-02 |
|---|---|
| F-01 (D3D PAL, tgames) | F-01 convierte EU→D3D; F-02 ya es D3D nativo (NTSC). No aplicable a EU. |
| F-03 (FPS Limiter EU) | Posible solape a comprobar (Fase 2): ¿F-02 incluye cap de FPS? |
| F-04 (supuesto exe capeado 30 FPS) | Probable duplicado descriptivo de F-02 (sin pieza separada). |
| F-07 (nGlide 0.99 Vista) | Procedimiento que usa `gex2_patch.zip` (F-02) como payload. |
| F-09 (Setup package NTSC) | Inventario 22 entradas; exe 2013 + 15 WAV; README leído. |
| F-11 (No-CD D3D) | Análogo funcional (No-CD), artefacto distinto. |
| F-13 (parche oficial 3dfx) | No aplicable (base D3D distinta de EU-Glide). |

**HIPÓTESIS:** F-02 y F-04 podrían ser el mismo binario (pendiente de
comparación binaria en Fase 2).

## 3. Inventario S-14 (2013 NTSC-D3D)

**CONFIRMADO:** inventario nominal completo (22 entradas), README leído íntegro.

| Componente | Detalle |
|---|---|
| `gex3d.exe` | "parcheado" 2013, NTSC-D3D |
| `music/` | 15 WAV (`track01–15`), 431.4 MB |
| Instaladores | nGlide 2.10, WinCDEmu 4.1, `_inmm` 2.3.8 |
| README declarado | Montar `Gex3DD3D.ccd` (D3D) + SETUP.EXE → copiar exe+`music/` → nGlide → música vía `_inmm` (DirectShow + `_inmm.ini`) → jugar sin disco |
| Rutas | `gex23d` (D3D) |
| Voces | US (Dana Gould) — `voice/` (no `voiceuk`) |

**UNKNOWN:** hashes del exe 2013 no verificados contra el inventario (pendiente).
**UNKNOWN:** procedencia del exe 2013 (quién lo parcheó, con qué herramientas).

## 4. Diferencias conocidas con el original EU

| Aspecto | Original EU | F-02 / exe 2013 |
|---|---|---|
| Renderer | Glide (3DFX) | Direct3D (D3D) |
| Región/voces | EU (Leslie Phillips, `voiceuk`) | NTSC (Dana Gould, `voice`) |
| CD-check | Presente (error frontal si falta CD) | No-CD (sin disco necesario) |
| Música | CD-audio (Red Book) | WAV (15 pistas, `_inmm`) |
| Timing | Acoplado a frame (16.666/frame) | **UNKNOWN** (no analizado) |
| Resolución | 512×384 @ 75 Hz (juego) | Desconocida |
| FPS cap | ~25 (inferido, 75/3) | Desconocido |
| Instalación | Instalador original + `.reg` | Manual (copiar + nGlide + `_inmm`) |

**CONFIRMADO:** diferencias de renderer, región, CD-check y música.
**UNKNOWN:** timing, resolución, FPS cap, y si el exe 2013 incluye el cap de 30 FPS.

## 5. Aplicabilidad a EU

**HIPÓTESIS:** F-02 NO es aplicable a la base EU-Glide.
- F-02 es NTSC-D3D; nuestra base es EU-Glide v1.00.000.
- Diferente renderer (D3D vs Glide), diferente región (voces US vs UK), diferente estructura de archivos (`gex23d` vs `gex2`).
- Sin evidencia de aplicabilidad a EU.

**CONFIRMADO:** el README de S-14 dice explícitamente "NTSC PC version", no EU.

## 6. Gaps pendientes (Fase 2)

1. **Diff binario** del exe 2013 vs original EU (pendiente: requiere Ghidra para call-graph).
2. **Verificar hashes** del exe 2013 contra el inventario S-14.
3. **Determinar si F-02 incluye cap de 30 FPS** (F-04 pendiente de fusión).
4. **Analizar timing** del exe 2013: ¿usa el mismo sello 16.666? ¿mismo acoplamiento?
5. **Verificar renderer** del exe 2013: ¿D3D nativo o wrapper?
6. **Comparar strings/imports** del exe 2013 vs original EU.
7. **Determinar procedencia** del exe 2013 (quién lo parcheó).

## 7. Referencias

- `PATCH_ANALYSIS.md` F-02, F-04, F-07, F-09, F-11, F-13
- `KNOWN_ISSUES.md` I-01, I-02, I-03, I-11, I-12, I-15, I-16
- `REVERSE_ENGINEERING.md` §2 (objetivos), §3 (plan por etapas)
- `RESEARCH.md` S-04, S-14
- `MODERNIZATION_GOALS.md` FA-01, FA-03, FA-12
- `docs/ORIGINAL_ARTIFACT_INVENTORY.md` (inventario de binarios)
- `docs/REA_WINDOWS_ENABLEMENT.md` (análisis profundo pendiente de Ghidra)
- `docs/F03_VS_ORIGINAL.md` (comparativa F-03 vs original)
