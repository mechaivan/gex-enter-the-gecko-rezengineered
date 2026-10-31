/*
 * gex/asset/sag.h - reader for the "SLAG" audio container of GEX3D.EXE (EU).
 *
 * Evidence (static; see docs/SAG_CONTAINER.md):
 *   0x0053899C  header check, chunk walk, "fmt " copy, "data" offset
 *   0x00538B01  read_u32: 4 bytes, or -1 on short read / 0xFFFFFFFF
 *   0x00538409  read(dst, n): file handle or in-memory buffer + cursor
 *   0x00538A99  tell()
 *   0x00538ACA  seek(absolute)
 *   0x005388AC  whole-file load: open, size, allocate, read, close
 *
 * The parser is a pure function over bytes: no allocation, no file I/O and no
 * DirectSound. `gex_sag_load` mirrors the observed whole-file load sequence.
 *
 * Fidelity: accept/reject rules and copied bytes follow the original. Two
 * deliberate additions are documented in docs/SAG_CONTAINER.md §6:
 *   - `data_complete` reports a truncated "data" payload instead of hiding it;
 *   - every access is bounds-checked with size_t (the original trusts the file).
 */

#ifndef GEX_ASSET_SAG_H
#define GEX_ASSET_SAG_H

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Size of the WAVEFORMATEX the original copies out of the "fmt " chunk. */
#define GEX_SAG_FORMAT_BYTES 18u

typedef enum gex_sag_status {
    GEX_SAG_OK = 0,
    GEX_SAG_ERR_ARGUMENT,  /* NULL pointer or missing output */
    GEX_SAG_ERR_IO,        /* file could not be opened, sized or read */
    GEX_SAG_ERR_HEADER,    /* no "SLAG" or no "WAVE" */
    GEX_SAG_ERR_SENTINEL,  /* a dword is 0xFFFFFFFF (the original's -1) */
    GEX_SAG_ERR_TRUNCATED, /* input ends before the declared structure */
    GEX_SAG_ERR_NO_DATA    /* the chunk walk reached the end without "data" */
} gex_sag_status;

typedef struct gex_sag {
    /* Second dword after "SLAG". The original only requires it to be != -1;
     * its meaning (RIFF-like form size?) is not established. */
    uint32_t form_size;

    /* Bytes of the last "fmt " chunk: min(declared size, 18), zero-padded.
     * WAVEFORMATEX layout: wFormatTag, nChannels, nSamplesPerSec,
     * nAvgBytesPerSec, nBlockAlign, wBitsPerSample, cbSize. */
    uint8_t format[GEX_SAG_FORMAT_BYTES];
    uint32_t format_chunk_size; /* declared size of the "fmt " chunk */
    size_t format_bytes;        /* bytes actually copied into format[] */
    int has_format;             /* a "fmt " chunk was seen before "data" */

    /* "data" chunk: declared size and offset of its payload. */
    size_t data_offset;
    uint32_t data_size;
    int data_complete; /* data_offset + data_size <= input size (added check) */
} gex_sag;

/*
 * Parse a whole container held in memory. On GEX_SAG_OK, `out` is fully
 * populated. On any error, `out` is zeroed. The function never reads outside
 * [data, data + size).
 */
gex_sag_status gex_sag_parse(const void *data, size_t size, gex_sag *out);

/* Stable English name for logs and tests ("ok", "truncated", ...). */
const char *gex_sag_status_name(gex_sag_status status);

/*
 * Load a whole file into a malloc'd buffer (mirrors 0x005388AC: open, seek to
 * end, size, seek to start, allocate, read, close). On success the caller owns
 * *data and must release it with gex_sag_release(). On failure *data is NULL.
 */
gex_sag_status gex_sag_load(const char *path, unsigned char **data, size_t *size);

/* Frees a buffer returned by gex_sag_load(). NULL is accepted. */
void gex_sag_release(void *data);

#ifdef __cplusplus
}
#endif

#endif /* GEX_ASSET_SAG_H */
