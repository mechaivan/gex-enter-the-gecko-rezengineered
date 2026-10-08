# COMPATIBILITY — Matriz de compatibilidad

> **Nivel de evidencia global: SIN DATOS PROPIOS.**
> Las notas proceden de fuentes secundarias (2026-10-08) y están sin verificar.
> Esta matriz se rellenará con resultados de testing propio en Fase 5.

## Versiones del juego

| Versión              | Región | Render de fábrica     | Evidencia      |
|----------------------|--------|-----------------------|----------------|
| PC retail US         | US     | Direct3D + Glide (?)  | PCGamingWiki   |
| PC retail EU         | EU/PAL | Glide/3Dfx (solo ?)   | PCGamingWiki   |
| PC retail FR (Pointsoft) | FR | (por determinar)      | Archive.org    |
| PC OEM Quantum3D Raven | US  | 3Dfx bundle           | Archive.org    |
| Demo PC (Toon TV)    | ?      | 3Dfx-only             | PCGW Community |

## Sistemas operativos (objetivo de testing futuro)

| OS            | Original | + Fixes comunitarios | REZengineered |
|---------------|----------|----------------------|---------------|
| Windows 95/98 | ?        | ?                    | N/A (objetivo: preservar comportamiento) |
| Windows XP    | ?        | Parche D3D era "for XP" | ?          |
| Windows 7/8   | ?        | ?                    | ?             |
| Windows 10/11 | No arranca sin fixes (?) | Manual install + F-01/F-02 | Objetivo principal |
| Linux + Wine  | Parcial (VOGONS) | Fix Indeo documentado | Objetivo secundario |

## Renderers / wrappers

| Ruta              | Estado reportado                         | Fuente        |
|-------------------|------------------------------------------|---------------|
| Glide + Voodoo2   | Referencia original (512x384@60)         | Foro Zeus     |
| Glide + nGlide    | Funciona; too fast sin cap; 75 Hz pedido | Foro Zeus     |
| D3D (US)          | Nativo en versión US                     | PCGamingWiki  |
| D3D (EU + F-01)   | Añadido por parche; cap 24 FPS (?)       | PCGamingWiki  |
| dgVoodoo2         | Crash con vorpX                          | PCGamingWiki  |

## Notas

- El comportamiento de referencia ("correcto") debe definirse per sistema:
  física, timing, velocidad de juego y render en hardware de época.
- Todo resultado futuro debe registrar: versión del juego, OS, GPU, wrapper,
  fix aplicado, FPS medidos y comportamiento observado.
