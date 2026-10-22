# ByteMatch — informe de progreso (Gex: Enter the Gecko, EU)

> **Nivel de evidencia:** mixto. Lo marcado **CONFIRMADO** sale del análisis directo del
> binario. Lo marcado **PENDIENTE** es trabajo que aún no existe en el repositorio.
> Fecha del análisis: 2026-10-21.

## 1. Qué es ByteMatch y cómo lee este repositorio

[ByteMatch](https://bytematch.dev/mechaivan/gex-enter-the-gecko-rezengineered) es un
rastreador de progreso de decompilación *matching*. No compila ni compara nada: lee el
informe que genera [`objdiff`](https://github.com/encounter/objdiff) y publica sus cifras.

El proyecto está registrado y su rama de lectura configurada es **`dev/gex`** (CONFIRMADO:
el propio sitio la muestra). ByteMatch solo acepta:

- ejecuciones de CI **correctas** (`conclusion == success`),
- sobre la rama configurada (`dev/gex`),
- disparadas por **push** (no por pull request),
- con un artefacto que case con `^(?<version>.+)_report$` → aquí **`EU_report`**,
- y que contenga un `report.json` legible según el esquema de informe de objdiff v2.

## 2. Estado actual: 0,00 % (verdadero, no un fallo)

`report/EU/report.json` es un informe **base** generado por
[`tools/bytematch-report.mjs`](../tools/bytematch-report.mjs) a partir del `GEX3D.EXE`
original. Todas sus cifras son medibles en el binario:

| Medida | Valor | Origen |
|---|---|---|
| `total_code` | **1 354 413 B** | suma de las 1 647 funciones identificadas en `.text` |
| `total_data` | **196 096 B** | bytes de `.rdata` + `.data` + `.idata` presentes en el fichero |
| `matched_code` | 0 B | nada reimplementado todavía |
| `matched_functions` | 0 | ídem |
| `total_functions` | **1 647** | puntos de entrada con llamada directa (`E8 rel32`) dentro de `.text` |
| `total_units` | **2** | `game/code` y `game/imports` |

En la página se verá **Code 1.29 MiB (0.00 %)**, **Data 191.5 KiB (0.00 %)**,
**Functions 0 / 1 647** y **Units 0 / 2**. Es la verdad: el proyecto no ha emparejado ni
una función.

### El mapa de unidades: qué se ve y por qué

ByteMatch no inventa el mapa: lo dibuja a partir de las **unidades** del informe. El nombre
de cada unidad es una ruta (`carpeta/subcarpeta/nombre`), y así se agrupan en recuadros; cada
recuadro mide el tamaño en bytes de esa unidad, y al abrirlo se listan sus funciones.

Hoy el informe tiene dos unidades, ambas medidas sobre el binario:

| Unidad | Funciones | Bytes | Qué es |
|---|---:|---:|---|
| `game/code` | 1 607 | 1 354 117 | el código del juego |
| `game/imports` | 40 | 296 | la capa de plataforma: *thunks* de importación |

Los 40 *thunks* de `game/imports` son stubs de la forma `jmp dword ptr [IAT]` que el
enlazador genera para cada función importada, y se nombran con el nombre real de la
importación: `_grBufferSwap@4`, `_grTexDownloadMipMap@16`, `_gu3dfLoad@8`,
`_grDrawTriangle@12`, `DirectSoundCreate`, `RtlUnwind`… No son código que haya que
reimplementar (el enlazador los regenera), pero sí forman parte de la función y conviene
verlos con su nombre.

**Por qué solo dos unidades y no una por subsistema:** porque el mapa de un proyecto de
decompilación real no sale del binario, sale del **árbol de fuentes de la reimplementación**.
Cada unidad es un archivo (o traducción) de nuestro C, y para compararla hace falta el
`.obj` original equivalente. Medidas sobre este binario (CONFIRMADO):

- El **grafo de llamadas es un bloque único**: 1 643 de las 1 647 funciones están conectadas
  entre sí. No hay forma de trocear el juego en subsistemas por el grafo.
- Solo **122** funciones llaman directamente a una API del sistema, y **todas** son *thunks*.
  El juego no tiene una capa deenvoltura propia: llama a Win32 y a Glide a través de la IAT.
- Los **huecos de relleno** entre funciones son de 8–15 B (107 en total, 1 227 B). El
  enlazador no dejó separadores detectables entre los `.obj` originales, así que los límites
  de los ficheros de origen tampoco son recuperables automáticamente.

Es decir: el troceado por subsistemas del juego es **trabajo de RE**, no una medida. Cuando el
RE identifique qué funciones pertenecen al renderer, a la lógica, al audio o a la
configuración, esas saldrán como unidades con nombre, y el mapa pasará a reflejar el juego
de verdad. Para eso hará falta además un `symbols.txt` (función → dirección → fichero
original), que es el artefacto que la comunidad de decomp mantiene a mano para binarios
*stripped* como este.

### Por qué se cuentan los bytes del fichero y no el tamaño virtual

Una sección PE declara dos tamaños: el **raw** (lo que ocupa en el fichero) y el **virtual**
(lo que reserva en memoria). En este binario la diferencia es enorme en `.data`:

| Sección | raw (fichero) | virtual (memoria) | diferencia |
|---|---:|---:|---:|
| `.text` | 1 354 752 | 1 354 462 | 290 B (relleno) |
| `.rdata` | 2 048 | 1 879 | 169 B |
| `.data` | **189 952** | **1 120 528** | **930 576 B** |
| `.idata` | 4 096 | 3 786 | 310 B |

Los ~930 KB de diferencia en `.data` **nunca estuvieron en el disco**: son memoria sin
inicializar (un buffer grande de runtime). Contarlos como «datos del juego» inflaba la cifra
en casi 1 MB. Por eso `total_code` y `total_data` usan el tamaño **raw**: lo que el
ejecutable ocupa de verdad.

Con eso, el reparto del fichero de 1 557 504 B queda así:

- `.text` (código) — 1 354 752 B — **87 %** (1 354 413 B en las funciones identificadas;
  339 B de relleno antes de la primera y después de la última)
- `.data` + `.rdata` + `.idata` (datos) — 196 096 B — **12,6 %**
- `.rsrc` (recursos) + cabeceras PE + overlay — 6 656 B — 0,4 %

Aun así, dentro de esos 196 096 B de datos solo **82 492 B** son distintos de cero: el resto
es relleno. La cifra cuenta los datos tal y como están en el fichero, sin filtrar los ceros,
porque un bloque con parte nula sigue siendo un bloque de datos que habría que reconstruir.

Los 196 096 B de datos **no están asignados a ninguna unidad**: los símbolos de datos no se
pueden atribuir a un fichero de origen sin el `symbols.txt`, así que se contabilizan a nivel
de proyecto. En cuanto exista, se repartirán entre las unidades que los posean.

### Cómo se cuentan las 1 647 funciones

El ejecutable está *stripped* (`PointerToSymbolTable = 0`) y no hay PDB, así que no existe
una tabla de símbolos que leer. Lo único localizable sin adivinar son los **puntos de entrada
con llamada directa**: cada `E8 rel32` de `.text` cuyo destino cae dentro de `.text` es un
punto de entrada al que el código original llama.

Son **1 647** direcciones distintas. El método solo puede *dejar fuera* funciones (las que
solo se alcanzan por caída o por un salto, o las que solo se referencian desde una tabla de
saltos), nunca inventar una: cada entrada listada es llamada de verdad por el binario. Por
eso la cifra es una **cota inferior** del total real, y no un total exacto.

El tamaño de cada función es la **distancia hasta la siguiente entrada identificada** (la
última llega hasta el final de `.text`). Es una **cota superior**: el relleno y las tablas de
saltos entre dos funciones se atribuyen a la anterior. Solo afecta al tamaño de los
recuadros del mapa, no a los porcentajes.

Cuando exista un informe real de objdiff, estos dos valores quedarán sustituidos por los que
calcule objdiff y dejarán de ser cotas.

## 3. Por qué todavía no hay un informe de objdiff

`objdiff` no genera progreso a partir de un `.EXE`. Necesita, **por unidad**:

1. `target_path` — el objeto original (el binario ya dividido en `.obj` por unidad),
2. `base_path` — el objeto compilado desde nuestro C, con el compilador original.

Hoy no existe ninguno de los dos:

- **PENDIENTE** divisor (*splitter*) que extraiga unidades del `.EXE`. Con el binario
  stripped y sin PDB hay que delimitar funciones a mano o por análisis.
- **PENDIENTE** `symbols.txt` con los límites y nombres de cada función, que es lo que
  permite asignar cada `.obj` a un fichero de origen y nombrar las unidades.
- **PENDIENTE** toolchain MSVC 5.0/6.0 reproducible (el enlazador del binario es 5.00).
  Documentado en [`docs/MSVC6_TOOLCHAIN.md`](MSVC6_TOOLCHAIN.md): empaquetado como
  release asset `msvc6-toolchain.zip` y workflow [`msvc6-build.yml`](../.github/workflows/msvc6-build.yml).
- **PENDIENTE** la primera unidad reimplementada en C.

Mientras falte cualquiera de los tres, `objdiff report generate` solo puede producir un
informe vacío; por eso el informe base se genera con nuestro script, que no inventa nada.

Lo que **sí** queda resuelto desde ahora es el andamiaje: cuando exista la primera unidad
real, se sustituye `report/EU/report.json` por la salida de objdiff manteniendo el nombre de
artefacto (`EU_report`) y el esquema v2, y todo lo demás (CI, ingesta, mapa) sigue igual.

## 4. Cómo se genera y se valida

El `.EXE` original **nunca se sube** al repositorio (material propietario, y `.gitignore`
ya excluye `*.exe`). Por eso la generación es local y el CI solo valida:

```bash
# Regenerar (verifica tamaño, MD5 y SHA-256 antes de escribir nada)
node tools/bytematch-report.mjs --exe /ruta/a/GEX3D.EXE

# Validar el fichero comprometido (lo que hace el CI)
node tools/bytematch-report.mjs --check
```

`--check` comprueba: esquema v2, las 16 medidas presentes, todos los `matched_*` a 0,
porcentajes coherentes con sus totales, que la suma de las unidades cuadre con el total, y
que la lista de funciones de las unidades case con `total_functions` y con los bytes
declarados.

## 5. Datos verificados del binario (CONFIRMADO)

`GEX3D.EXE` — EU, Glide:

- Tamaño: **1 557 504 B**
- MD5: `692b12825003417cc4a5a9d4db13eebe`
- SHA-256: `d0932549613f9c08fb895c5f8a6461ce044e1863b4cba42da4715c2b41e73886`
- PE32, `machine = 0x014C` (i386), `ImageBase = 0x00400000`
- Punto de entrada: **`0x00541B20`** (RVA `0x141B20`)
- Secciones (5): `.text`, `.rdata`, `.data`, `.idata`, `.rsrc` — **sin `.reloc`**
  (el directorio de reubicaciones base está vacío: reubicaciones eliminadas)
- Sin tabla de símbolos COFF
- Directorio de depuración: CodeView `NB10` →
  `C:\gex3d\game\3dfx\release_europe\gex3d.pdb` (PDB **no disponible**), más un registro
  `MISC` con la ruta de compilación y una tabla `FPO`
- Importaciones: **132** funciones de 7 DLL — `KERNEL32` (61), `glide2x` (38), `USER32` (20),
  `WINMM` (7), `ADVAPI32` (4), `GDI32` (1), `DSOUND` (1)

## 6. Corrección de la configuración anterior

Los ficheros `config/target.toml`, `config/match-units.toml`, `objdiff.yml` y
`config/README.md` se eliminaron en este cambio porque **no eran válidos**:

- `objdiff` no lee TOML: busca `objdiff.json`/`objdiff.yml` con el esquema `ProjectConfig`
  (`units[]` con `target_path`/`base_path`). El `objdiff.yml` que había usaba claves
  inventadas (`version`, `target`, `build`, `report`).
- `objdiff report generate` no acepta `--config`, `--units` ni `--version`. Su interfaz real
  es `[-p <project>] [-o <output>] [-f <format>] [-c <config>...]` (verificado con
  objdiff 3.8.3). Ese fue el motivo de que fallaran todas las ejecuciones de CI anteriores.
- `config/target.toml` no coincidía con el binario: declaraba 6 secciones (incluida
  `.reloc`), `entry_point = 0x00463278` (real: `0x00541B20`), `relocations_stripped = false`
  (real: eliminadas) y confundía tamaños *raw* con *virtual*.
- `config/match-units.toml` listaba reubicaciones y una función (`0x0043E1BA`) deducidas por
  inferencia, nunca verificadas, y un paso de build `python scripts/build_exact.py` que no
  existe.
- `config/README.md` indicaba `pip install objdiff-cli`; objdiff **no** está en PyPI ni en
  crates.io: solo se distribuye como binario en sus *releases* de GitHub.

El análisis de la función `0x0043E1BA` sigue siendo válido como documentación de RE en
[`docs/TIMING_GAME_LOOP_ANALYSIS.md`](TIMING_GAME_LOOP_ANALYSIS.md); lo que se retira es su
uso como unidad de objdiff, que nunca estuvo verificado.

## 7. Qué hará subir el porcentaje

Sustituir `report/EU/report.json` por la salida de objdiff cuando exista una primera unidad
real, manteniendo el mismo nombre de artefacto (`EU_report`) y el mismo esquema. A partir de
ahí el porcentaje sube solo con cada push a `dev/gex` que empareje código.
