# BUILD — Construcción

> Estado: **pendiente de definir** (Fase 1: no hay nada que construir).

Cuando exista implementación (Fase 4), este documento describirá:

- requisitos (compiladores, SDKs, dependencias);
- pasos de build reproducibles;
- qué produce cada target (loader, DLL, parches…);
- cómo el usuario aporta sus archivos originales;
- verificación de artefactos (hashes).

De momento, ver [ROADMAP.md](ROADMAP.md) y [REVERSE_ENGINEERING.md](REVERSE_ENGINEERING.md).

## Distribución futura (planificación 2026-10-09 — NO implementado)

> Requisitos para el futuro instalador/validador/lanzador, cuando el diseño
> técnico esté decidido (Fase 3+). Nada de esto existe todavía.

- Distribuir solo mejoras y herramientas propias; jamás el exe original ni
  archivos protegidos (música, vídeos, texturas, niveles, recursos, ISOs,
  instaladores originales, DLL propietarias ni paquetes con recursos).
- No descargar, alojar ni distribuir automáticamente los originales en
  nombre del usuario: el futuro instalador pedirá la ubicación de su copia.
- Validación local (hashes/versión) con mensajes claros si falta algo o la
  versión es incompatible; sin subir archivos a servidores.
- Documentar requisitos de la copia original, instalación, copias de
  seguridad, restauración y desinstalación para otros usuarios.
- Separar en el diseño: código/herramientas/docs propios vs archivos del
  juego (el usuario los aporta; ver LICENSE.md §3).
