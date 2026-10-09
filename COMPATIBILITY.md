# COMPATIBILITY — Matriz de compatibilidad

> **Nivel de evidencia global: PRIMEROS DATOS PROPIOS (Fase 1, solo
> estáticos).** Sin testing todavía: las notas no marcadas proceden de
> fuentes secundarias (2026-10-08) y están sin verificar. Esta matriz se
> rellenará con resultados de testing propio desde Fase 4/5.

## Versiones del juego

| Versión              | Región | Render de fábrica     | Evidencia      |
|----------------------|--------|-----------------------|----------------|
| PC retail US         | US     | Direct3D + Glide (?)  | PCGamingWiki   |
| PC retail EU         | EU/PAL | Glide/3Dfx (solo)     | Inventario Fase 1 (CONFIRMED: 38 imports glide2x, 0 D3D) |
| PC retail FR (Pointsoft) | FR | (por determinar)      | Archive.org    |
| PC OEM Quantum3D Raven | US  | 3Dfx bundle           | Archive.org    |
| PC versión D3D (carpeta `gex23d`) | ? | Direct3D         | tgames (instalación distinguida de `gex23dfx`; relación con retail US: UNKNOWN) |
| Demo PC (Toon TV)    | ?      | 3Dfx-only             | PCGW Community |

## Sistemas operativos (objetivo de testing futuro)

| OS            | Original | + Fixes comunitarios | REZengineered |
|---------------|----------|----------------------|---------------|
| Windows 95/98 | ?        | ?                    | N/A (objetivo: preservar comportamiento) |
| Windows XP    | ?        | Parche D3D era "for XP" | ?          |
| Windows 7/8   | ?        | ?                    | ?             |
| Windows 10/11 | No arranca sin fixes (?) | Manual install + F-01/F-02 | Objetivo principal |
| Linux + Wine  | Parcial (VOGONS) | Fix Indeo documentado | DEFERRED — X-01, secundario, no objetivo |

## Renderers / wrappers

| Ruta              | Estado reportado                         | Fuente        |
|-------------------|------------------------------------------|---------------|
| Glide + Voodoo2   | Referencia original (512x384@60)         | Foro Zeus     |
| Glide + nGlide    | Funciona; too fast sin cap; 75 Hz pedido | Foro Zeus     |
| D3D (US)          | Nativo en versión US                     | PCGamingWiki  |
| D3D (EU + F-01)   | Añadido por parche; cap 24 FPS (?)       | PCGamingWiki  |
| dgVoodoo2         | Crash con vorpX                          | PCGamingWiki  |

## Dimensiones futuras de la matriz (sin resultados)

Cuando haya testing propio, cubrir: resoluciones, refresh rates, aspect
ratios, fullscreen / borderless / windowed, multi-monitor, DPI (100–200%),
APIs de mando (DirectInput legacy / XInput / DualShock), fabricantes de GPU
(AMD / NVIDIA / Intel, iGPU/dGPU) y configuraciones modernas de Windows.

## Matriz de hardware (estructura, Fase 13 — vacía)

> Herramienta de QA/documentación, no feature. NO rellenar con datos
> inventados. Ejemplo de formato (ficticio, solo ilustra columnas):

| GPU | Fabricante | iGPU/dGPU | Windows | Driver | Renderer | Resolución | Refresh | Modo | Mando | Resultado | FPS | Estabilidad | Problemas | Workaround | REZ ver. |
|---|---|---|---|---|---|---|---|---|---|---|---|---|---|---|---|
| *(ejemplo)* | AMD | dGPU | 11 | — | Modern | 1080p | 144 Hz | borderless | XInput | ✅ | — | — | — | — | — |

## Notas

- Datos propios Fase 1 (binario EU v1.00.000): Glide exclusivo; joystick vía
  WinMM (`joyGetPosEx`, sin DirectInput); audio vía WinMM/DSound + CD-DA por
  MCI (`mciSendCommandA`); CD 1 datos + 16 pistas audio. Ver inventario.
- El comportamiento de referencia ("correcto") debe definirse per sistema:
  física, timing, velocidad de juego y render en hardware de época.
- Todo resultado futuro debe registrar: versión del juego, OS, GPU, wrapper,
  fix aplicado, FPS medidos y comportamiento observado.
- Lote 1 (2026-10-09): Tgames distingue instalaciones `gex23dfx` (3DFX) y
  `gex23d` (D3D); F-10 solo funciona en D3D o 3DFX-parcheada-a-D3D; F-12
  confirma menú debug en la build D3D.
- M-25 (PROPOSED): set de voces USA de PC (`voice/`) sin verificar; al
  testear audio registrar siempre la variante regional (UK/USA).
