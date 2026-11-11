/*
 * gex/vfx.h - contenedor de texturas .VFX de Gex PC (Módulo 4A, portable).
 *
 * El formato NO se deduce de herramientas externas: está fijado por el
 * ejecutable original (GEX3D.EXE) y por los ficheros originales. Toda la
 * evidencia y su origen están en docs/VFX_TEXTURE_FORMAT.md.
 *
 * Lo que está DEMOSTRADO (verifiable en el ejecutable y en los ficheros):
 *
 *   +0x00  dword   número de texturas del fichero
 *   +0x04  ...     tabla de registros, uno detrás de otro
 *
 *   Cada registro:
 *   +0x00  dword   campo A  (2, 3, 4, 6 en los ficheros originales)
 *   +0x04  dword   campo B  (igual que A en todos los ficheros originales)
 *   +0x08  dword   campo C  (2..6)
 *   +0x0C  dword   FORMATO de la textura (1, 11 o 12 en los originales)
 *   +0x10  dword   puntero a los datos: lo rellena el juego, el fichero solo
 *                   guarda un valor de relleno que hay que IGNORAR
 *   +0x14  ...     datos propios del formato (para el formato 1, la tabla de
 *                   paleta que el juego sube con grTexDownloadTable)
 *   +0x84  dword   tamaño en bytes del bloque de datos
 *   +0x88  dword   escrito por el juego (cursor del pool de Glide)
 *   +0x8C  bytes  bloque de datos de la textura
 *
 *   El registro siguiente está en registro + 0x8C + dword[+0x84].
 *
 *   El recorrido de los `count` registros tiene que cubrir el fichero
 *   EXACTO (en los 37 .VFX del inventario el último bloque termina en el
 *   EOF): si sobran bytes, el contenedor se rechaza con GEX_VFX_ERR_FORMAT.
 *
 * Lo que NO está demostrado (y por tanto NO se implementa): el formato de
 * píxel de cada FORMATO, las dimensiones y la semántica exacta de la paleta.
 * Ver docs/VFX_TEXTURE_FORMAT.md §4.
 *
 * Portable C11: sin Windows, sin D3D11.
 */

#ifndef GEX_VFX_H
#define GEX_VFX_H

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Tamaño de la cabecera de cada registro (los datos empiezan en +0x8C). */
#define GEX_VFX_RECORD_HEADER_SIZE 0x8Cu

/* Offset del tamaño del bloque de datos dentro del registro. */
#define GEX_VFX_RECORD_SIZE_OFFSET 0x84u

/* Offset del bloque de datos dentro del registro. */
#define GEX_VFX_RECORD_DATA_OFFSET 0x8Cu

/* Offset del puntero a los datos (lo escribe el juego; en el fichero hay un
 * valor de relleno que hay que ignorar). */
#define GEX_VFX_RECORD_POINTER_OFFSET 0x10u

/* Offset del formato de la textura. */
#define GEX_VFX_RECORD_FORMAT_OFFSET 0x0Cu

/* Offset de la tabla de paleta del formato 1 (la sube grTexDownloadTable). */
#define GEX_VFX_RECORD_PALETTE_OFFSET 0x14u

/* Tamaño del bloque de paleta: desde +0x14 hasta +0x84 (0x70 B). Es el
 * tamaño que el juego copia entre entradas que comparten paleta
 * (`memcpy` de 0x70 B en `0x004732FB`). */
#define GEX_VFX_PALETTE_BYTES (GEX_VFX_RECORD_SIZE_OFFSET - \
                               GEX_VFX_RECORD_PALETTE_OFFSET)

/* Formatos observados en los ficheros originales. Son los identificadores que
 * el juego usa en su propio campo de formato (`GrTexInfo.format`, +0x0C) y
 * con los que decide la paleta y el blending alfa. **No** son los de la
 * enumeración GR_TEXFMT de Glide: el juego trata 1, 11 y 12 de forma propia
 * (docs/VFX_TEXTURE_FORMAT.md §5.3). El FORMATO DE PÍXEL de cada uno sigue
 * sin estar demostrado. */
#define GEX_VFX_FORMAT_1  1u  /* con tabla de paleta en +0x14 */
#define GEX_VFX_FORMAT_11 11u
#define GEX_VFX_FORMAT_12 12u /* el juego activa el blending alfa con este */
#define GEX_VFX_FORMAT_0  0u  /* el juego lo distingue con otro modo de
                                * alfa; no aparece en los .VFX originales */

/* Un registro del contenedor. */
typedef struct gex_vfx_record {
    uint32_t field_a;         /* +0x00 */
    uint32_t field_b;         /* +0x04 */
    uint32_t field_c;         /* +0x08 */
    uint32_t format;          /* +0x0C */
    uint32_t stored_pointer;  /* +0x10 (valor de relleno: se ignora) */
    uint64_t file_offset;     /* offset del registro dentro del fichero */
    const uint8_t *data;      /* bloque de datos (+0x8C), NULL si size == 0 */
    uint32_t data_size;       /* +0x84 */
    /* Tabla de paleta: SOLO para el formato 1 (el único que el juego sube
     * con grTexDownloadTable). Para los formatos 11 y 12 vale NULL y
     * palette_size vale 0. */
    const uint8_t *palette;
    uint32_t palette_size;    /* bytes desde +0x14 hasta +0x84 (solo formato 1) */
} gex_vfx_record;

/* Un fichero .VFX abierto. */
typedef struct gex_vfx_file {
    uint8_t *data;            /* el fichero completo (propiedad del módulo) */
    size_t size;              /* su tamaño en bytes */
    uint32_t declared_count;  /* dword de la cabecera */
    gex_vfx_record *records;  /* tabla de registros (propiedad del módulo) */
    uint32_t record_count;    /* registros realmente recorridos */
} gex_vfx_file;

typedef enum gex_vfx_status {
    GEX_VFX_OK = 0,
    GEX_VFX_ERR_ARG,      /* ruta o puntero nulo */
    GEX_VFX_ERR_OPEN,     /* no se pudo abrir o leer el fichero */
    GEX_VFX_ERR_FORMAT,   /* cabecera incoherente, o sobran bytes tras la tabla */
    GEX_VFX_ERR_RANGE,    /* un offset o un tamaño sale del fichero */
    GEX_VFX_ERR_OVERFLOW, /* una multiplicación desborda */
    GEX_VFX_ERR_NOMEM     /* falta memoria */
} gex_vfx_status;

/*
 * Abre y analiza un .VFX. El fichero se lee entero en memoria (solo lectura)
 * y los registros apuntan dentro de ese bloque.
 */
gex_vfx_status gex_vfx_open(gex_vfx_file *file, const char *path);

/*
 * Analiza un .VFX ya en memoria (útil para pruebas y para ficheros
 * embebidos). `data` se COPIA: el módulo no toma elOwnership.
 */
gex_vfx_status gex_vfx_parse(gex_vfx_file *file, const uint8_t *data,
                              size_t size);

/* Libera todo. Idempotente. */
void gex_vfx_close(gex_vfx_file *file);

const char *gex_vfx_status_name(gex_vfx_status status);

/*
 * Devuelve el índice del registro con ese índice de textura de 14 bits, o
 * -1 si el índice está fuera de rango. El índice de 14 bits es el que
 * guarda la geometría del nivel (word del registro UV, `& 0x3FFF`) y es el
 * que indexa esta tabla en el original.
 */
int gex_vfx_find_index(const gex_vfx_file *file, uint32_t index14);

/*
 * ¿El formato tiene una tabla de paleta en +0x14? Solo el formato 1 la
 * tiene (el juego la sube con grTexDownloadTable). Devuelve 1 o 0.
 */
int gex_vfx_format_has_palette(uint32_t format);

/*
 * Datos demostrados sobre cada FORMATO (ver docs/VFX_TEXTURE_FORMAT.md §4):
 * qué hace el juego con él. NO incluye el formato de píxel, que no está
 * demostrado.
 */
typedef struct gex_vfx_format_info {
    uint32_t format;        /* el identificador tal cual */
    int has_palette;        /* el juego sube una paleta desde +0x14 */
    int enables_alpha;      /* el juego activa el blending alfa con él */
    uint32_t palette_bytes; /* tamaño del bloque de paleta (0x70) o 0 */
} gex_vfx_format_info;

/*
 * Rellena `out` con los datos demostrados del formato. Devuelve 1 si el
 * formato es uno de los observados en los ficheros originales (1, 11, 12) y 0
 * si no se conoce nada de él.
 */
int gex_vfx_format_info_get(uint32_t format, gex_vfx_format_info *out);

/*
 * Entrada que comparte la paleta con `index`.
 *
 * El juego copia el bloque de paleta (0x70 B) de una entrada a otra cuando
 * dos texturas usan la misma paleta (`memcpy` en `0x004732FB`), de modo que
 * solo tiene sentido entre entradas con bloques idénticos. Devuelve el índice
 * de la otra entrada, o -1 si no hay ninguna (o si `index` no tiene paleta).
 */
int gex_vfx_palette_twin(const gex_vfx_file *file, uint32_t index);

#ifdef __cplusplus
}
#endif

#endif /* GEX_VFX_H */
