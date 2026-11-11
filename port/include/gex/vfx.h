/*
 * gex/vfx.h - contenedor de texturas .VFX de Gex PC (Módulos 4A y 4B).
 *
 * El formato NO se deduce de herramientas externas: está fijado por el
 * ejecutable original (GEX3D.EXE), por los ficheros originales y por el SDK
 * público de Glide 2.x. Toda la evidencia y su origen están en
 * docs/VFX_TEXTURE_FORMAT.md.
 *
 * Lo que está DEMOSTRADO (verificable en el ejecutable, en los ficheros
 * originales y en el SDK de Glide 2.x):
 *
 *   +0x00  dword   número de texturas del fichero
 *   +0x04  ...     tabla de registros, uno detrás de otro
 *
 *   Cada registro es un `GrTexInfo` de Glide 2.x:
 *   +0x00  dword   smallLod     (GrLOD_t: 0 = 256 px ... 8 = 1 px)
 *   +0x04  dword   largeLod     (GrLOD_t)
 *   +0x08  dword   aspectRatio  (GrAspectRatio_t: 0 = 8x1 ... 6 = 1x8)
 *   +0x0C  dword   format       (GrTextureFormat_t de Glide 2.x)
 *   +0x10  dword   puntero a los datos: lo rellena el juego, el fichero solo
 *                   guarda un valor de relleno que hay que IGNORAR
 *   +0x14  ...     tabla NCC (GuNccTable, 112 B) SOLO en el formato YIQ_422
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
 * Formatos de píxel (identificadores GR_TEXFMT de Glide 2.x, los mismos
 * valores que el juego pasa a Glide):
 *
 *   1  = GR_TEXFMT_YIQ_422    1 byte por texel, comprimido con la tabla NCC
 *   11 = GR_TEXFMT_ARGB_1555  2 bytes por texel (A1 R5 G5 B5)
 *   12 = GR_TEXFMT_ARGB_4444  2 bytes por texel (A4 R4 G4 B4)
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

/* Offset de la tabla NCC del formato YIQ_422 (la sube grTexDownloadTable con
 * GR_TEXTABLE_NCC0). */
#define GEX_VFX_RECORD_NCC_TABLE_OFFSET 0x14u

/* Tamaño de la tabla NCC: desde +0x14 hasta +0x84 (0x70 = 112 B). Es el
 * tamaño exacto de un `GuNccTable` de Glide 2.x y el que el juego copia
 * entre dos entradas que comparten tabla (`memcpy` de 0x70 B en
 * `0x004732FB`). */
#define GEX_VFX_NCC_TABLE_BYTES (GEX_VFX_RECORD_SIZE_OFFSET - \
                                 GEX_VFX_RECORD_NCC_TABLE_OFFSET)

/* Formatos de píxel (GR_TEXFMT de Glide 2.x). Los valores coinciden con los
 * que el juego entrega a Glide en `GrTexInfo.format` (+0x0C). */
#define GEX_VFX_FORMAT_RGB_332   0u  /* el juego lo distingue con otro modo de
                                      * alfa; no aparece en los .VFX */
#define GEX_VFX_FORMAT_YIQ_422   1u  /* con tabla NCC en +0x14 */
#define GEX_VFX_FORMAT_ARGB_1555 11u
#define GEX_VFX_FORMAT_ARGB_4444 12u /* el juego activa el blending alfa */

/* Un registro del contenedor. */
typedef struct gex_vfx_record {
    uint32_t small_lod;       /* +0x00 (GrLOD_t) */
    uint32_t large_lod;       /* +0x04 (GrLOD_t) */
    uint32_t aspect_ratio;    /* +0x08 (GrAspectRatio_t) */
    uint32_t format;          /* +0x0C (GrTextureFormat_t) */
    uint32_t stored_pointer;  /* +0x10 (valor de relleno: se ignora) */
    uint64_t file_offset;     /* offset del registro dentro del fichero */
    const uint8_t *data;      /* bloque de datos (+0x8C), NULL si size == 0 */
    uint32_t data_size;       /* +0x84 */
    /* Tabla NCC (GuNccTable): SOLO para el formato YIQ_422 (el único que el
     * juego sube con grTexDownloadTable). Para los demás formatos vale NULL
     * y ncc_table_size vale 0. */
    const uint8_t *ncc_table;
    uint32_t ncc_table_size;  /* bytes desde +0x14 hasta +0x84 */
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
 * embebidos). `data` se COPIA: el módulo no toma el ownership.
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
 * ¿El formato lleva una tabla NCC en +0x14? Solo el formato YIQ_422 la
 * lleva (el juego la sube con grTexDownloadTable). Devuelve 1 o 0.
 */
int gex_vfx_format_has_ncc_table(uint32_t format);

/*
 * Datos demostrados sobre cada FORMATO (ver docs/VFX_TEXTURE_FORMAT.md §6):
 * qué hace el juego con él y de qué tipo es el píxel.
 */
typedef struct gex_vfx_format_info {
    uint32_t format;          /* el identificador tal cual */
    const char *name;         /* nombre del formato en Glide 2.x */
    int has_ncc_table;        /* el juego sube una tabla NCC desde +0x14 */
    int enables_alpha;        /* el juego activa el blending alfa con él */
    uint32_t ncc_table_bytes; /* tamaño del bloque NCC (112) o 0 */
    uint32_t bytes_per_texel; /* bytes por texel en memoria (1 o 2) */
    int decodable;            /* hay decodificador CPU en este módulo */
} gex_vfx_format_info;

/*
 * Rellena `out` con los datos demostrados del formato. Devuelve 1 si el
 * formato es uno de los observados en los ficheros originales (1, 11, 12) y 0
 * si no se conoce nada de él.
 */
int gex_vfx_format_info_get(uint32_t format, gex_vfx_format_info *out);

/*
 * Entrada que comparte la tabla NCC con `index`.
 *
 * El juego copia el bloque NCC (0x70 B) de una entrada a otra cuando dos
 * texturas usan la misma tabla (`memcpy` en `0x004732FB`), de modo que solo
 * tiene sentido entre entradas con bloques idénticos. Devuelve el índice de
 * la otra entrada, o -1 si no hay ninguna (o si `index` no tiene tabla).
 */
int gex_vfx_ncc_table_twin(const gex_vfx_file *file, uint32_t index);

/* ------------------------------------------------------------------ */
/* Tabla NCC (GuNccTable de Glide 2.x)                                 */
/* ------------------------------------------------------------------ */

/*
 * Tabla de descompresión NCC ("Narrow Channel Compression") de Glide 2.x.
 * Ocupa exactamente 112 bytes en memoria: 16 niveles de luminancia, 4
 * vectores de croma I, 4 vectores de croma Q y la forma empaquetada que
 * espera el hardware.
 */
typedef struct gex_vfx_ncc_table {
    uint8_t y_rgb[16];       /* 16 niveles de luminancia (0..255) */
    int16_t i_rgb[4][3];     /* croma I: 4 vectores, 9 bits con signo */
    int16_t q_rgb[4][3];     /* croma Q: 4 vectores, 9 bits con signo */
    uint32_t packed_data[12];/* forma empaquetada para el hardware */
} gex_vfx_ncc_table;

/*
 * Interpreta los 112 bytes crudos de una tabla NCC.
 *
 * Los 16 primeros bytes son los niveles de luminancia. Los 24 siguientes
 * son 12 valores de 16 bits con signo (4 vectores de 3 componentes) y los
 * 48 últimos son la forma empaquetada. Devuelve 1 si se pudo interpretar.
 */
int gex_vfx_ncc_table_parse(const uint8_t *raw, size_t size,
                             gex_vfx_ncc_table *out);

/*
 * Recalcula la forma empaquetada (`packed_data`) de una tabla a partir de
 * sus campos, con el mismo criterio que usa el SDK de Glide 2.x: los 16
 * niveles de luminancia en 4 palabras, y los 12 valores de cada vector de
 * croma en 3 palabras de 9 bits.
 */
void gex_vfx_ncc_table_pack(const gex_vfx_ncc_table *table,
                            uint32_t packed[12]);

/* ------------------------------------------------------------------ */
/* Dimensiones y decodificación                                         */
/* ------------------------------------------------------------------ */

/*
 * Dimensiones y tamaño deducidos de un registro, con las reglas de Glide
 * 2.x: el LOD fija el lado mayor (256 >> lod) y la relación de aspecto lo
 * divide (8x1, 4x1, 2x1, 1x1, 1x2, 1x4, 1x8). La cadena de mipmap va de
 * `large_lod` (el nivel más detallado) a `small_lod`.
 */
typedef struct gex_vfx_texture_info {
    uint32_t format;          /* GrTextureFormat_t del registro */
    uint32_t small_lod;       /* +0x00 */
    uint32_t large_lod;       /* +0x04 */
    uint32_t aspect_ratio;    /* +0x08 */
    uint32_t width;           /* ancho del nivel más detallado */
    uint32_t height;          /* alto del nivel más detallado */
    uint32_t levels;          /* número de niveles mipmap (>= 1) */
    uint32_t bytes_per_texel; /* 1 (formatos de 8 bits) o 2 (de 16 bits) */
    uint32_t total_bytes;     /* bytes esperados del bloque de datos */
    int has_ncc_table;        /* el registro lleva tabla NCC */
} gex_vfx_texture_info;

/*
 * Rellena `out` con las dimensiones y el tamaño esperados de la textura.
 * Devuelve 1 si el registro es coherente (LOD y aspecto dentro de rango,
 * formato conocido) y 0 si no lo es.
 */
int gex_vfx_texture_info_get(const gex_vfx_record *rec,
                             gex_vfx_texture_info *out);

/*
 * Tamaño de un nivel de la cadena (0 = el más detallado). Devuelve 1 si el
 * nivel existe.
 */
int gex_vfx_texture_level_size(const gex_vfx_texture_info *info,
                                uint32_t level, uint32_t *width,
                                uint32_t *height);

/* Offset del primer byte de un nivel dentro del bloque de datos. */
uint32_t gex_vfx_texture_level_offset(const gex_vfx_texture_info *info,
                                       uint32_t level);

/* Estado de una decodificación. */
typedef enum gex_vfx_decode_status {
    GEX_VFX_DEC_OK = 0,
    GEX_VFX_DEC_ERR_ARG,      /* puntero nulo o dimensión inválida */
    GEX_VFX_DEC_ERR_FORMAT,   /* formato de píxel sin decodificador */
    GEX_VFX_DEC_ERR_RANGE,    /* datos truncados o destino insuficiente */
    GEX_VFX_DEC_ERR_OVERFLOW  /* una multiplicación desborda */
} gex_vfx_decode_status;

const char *gex_vfx_decode_status_name(gex_vfx_decode_status status);

/*
 * Decodificadores CPU. La salida es siempre RGBA8 (4 bytes por píxel en el
 * orden R, G, B, A), con alfa recto (no premultiplicado). Los formatos de 8
 * bits (YIQ_422) devuelven alfa 255.
 *
 * `src_size` y `dst_size` se comprueban: si no hay bytes suficientes se
 * devuelve GEX_VFX_DEC_ERR_RANGE y no se escribe nada.
 */
gex_vfx_decode_status gex_vfx_decode_argb1555(const uint8_t *src,
                                               size_t src_size, uint32_t width,
                                               uint32_t height, uint8_t *dst,
                                               size_t dst_size);

gex_vfx_decode_status gex_vfx_decode_argb4444(const uint8_t *src,
                                               size_t src_size, uint32_t width,
                                               uint32_t height, uint8_t *dst,
                                               size_t dst_size);

gex_vfx_decode_status gex_vfx_decode_yiq422(const uint8_t *src, size_t src_size,
                                            const gex_vfx_ncc_table *ncc,
                                            uint32_t width, uint32_t height,
                                            uint8_t *dst, size_t dst_size);

/*
 * Decodifica un nivel de la textura de un registro (0 = el más detallado),
 * usando la tabla NCC del propio registro cuando el formato la necesita.
 */
gex_vfx_decode_status gex_vfx_decode_level(const gex_vfx_record *rec,
                                            const gex_vfx_texture_info *info,
                                            uint32_t level, uint8_t *dst,
                                            size_t dst_size);

#ifdef __cplusplus
}
#endif

#endif /* GEX_VFX_H */
