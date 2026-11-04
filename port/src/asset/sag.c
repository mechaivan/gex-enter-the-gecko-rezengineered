/*
 * sag.c - "SLAG" container reader. See gex/asset/sag.h for the evidence map.
 *
 * The control flow below is a direct transcription of 0x0053899C:
 *
 *   read4 != "SLAG"           -> -1
 *   read4 == -1               -> -1
 *   read4 != "WAVE"           -> -1
 *   loop:
 *     tag = read4; tag == -1  -> -1
 *     len = read4; len == -1  -> -1
 *     pos = tell()
 *     tag == "fmt ":  memset(dst,0,18); n=min(len,18); read(dst,n)==n else -1
 *     tag == "data":  return len
 *     seek(pos + len); repeat
 *
 * The only intentional differences are bounds checks (documented in
 * docs/SAG_CONTAINER.md §6) and the split between "short read" and "the
 * sentinel 0xFFFFFFFF", which the original merges because its read4() returns
 * -1 for both.
 */

#include "gex/asset/sag.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define TAG4(a, b, c, d) \
    ((uint32_t)(a) | ((uint32_t)(b) << 8) | ((uint32_t)(c) << 16) | ((uint32_t)(d) << 24))

#define SENTINEL 0xFFFFFFFFu

static uint32_t read_u32(const unsigned char *p)
{
    return (uint32_t)p[0] | ((uint32_t)p[1] << 8) | ((uint32_t)p[2] << 16) |
           ((uint32_t)p[3] << 24);
}

gex_sag_status gex_sag_parse(const void *data, size_t size, gex_sag *out)
{
    if (data == NULL || out == NULL) {
        return GEX_SAG_ERR_ARGUMENT;
    }

    memset(out, 0, sizeof(*out));

    const unsigned char *bytes = (const unsigned char *)data;

    if (size < 12) {
        return GEX_SAG_ERR_TRUNCATED;
    }
    if (read_u32(bytes) != TAG4('S', 'L', 'A', 'G')) {
        return GEX_SAG_ERR_HEADER;
    }

    out->form_size = read_u32(bytes + 4);
    if (out->form_size == SENTINEL) {
        return GEX_SAG_ERR_SENTINEL;
    }
    if (read_u32(bytes + 8) != TAG4('W', 'A', 'V', 'E')) {
        return GEX_SAG_ERR_HEADER;
    }

    size_t pos = 12;
    for (;;) {
        if (pos == size) {
            return GEX_SAG_ERR_NO_DATA;
        }
        if (size - pos < 8) {
            return GEX_SAG_ERR_TRUNCATED;
        }

        const uint32_t tag = read_u32(bytes + pos);
        const uint32_t chunk_size = read_u32(bytes + pos + 4);
        if (tag == SENTINEL || chunk_size == SENTINEL) {
            return GEX_SAG_ERR_SENTINEL;
        }

        const size_t payload = pos + 8;

        if (tag == TAG4('f', 'm', 't', ' ')) {
            const size_t copy = chunk_size < GEX_SAG_FORMAT_BYTES
                                    ? (size_t)chunk_size
                                    : (size_t)GEX_SAG_FORMAT_BYTES;
            if (size - payload < copy) {
                return GEX_SAG_ERR_TRUNCATED;
            }
            memset(out->format, 0, GEX_SAG_FORMAT_BYTES);
            if (copy != 0) {
                memcpy(out->format, bytes + payload, copy);
            }
            out->format_bytes = copy;
            out->format_chunk_size = chunk_size;
            out->has_format = 1;

            if ((size_t)chunk_size > size - payload) {
                return GEX_SAG_ERR_TRUNCATED;
            }
            pos = payload + (size_t)chunk_size;
            continue;
        }

        if (tag == TAG4('d', 'a', 't', 'a')) {
            out->data_offset = payload;
            out->data_size = chunk_size;
            out->data_complete = (size_t)chunk_size <= size - payload;
            return GEX_SAG_OK;
        }

        if ((size_t)chunk_size > size - payload) {
            return GEX_SAG_ERR_TRUNCATED;
        }
        pos = payload + (size_t)chunk_size;
    }
}

const char *gex_sag_status_name(gex_sag_status status)
{
    switch (status) {
    case GEX_SAG_OK:
        return "ok";
    case GEX_SAG_ERR_ARGUMENT:
        return "argument";
    case GEX_SAG_ERR_IO:
        return "io";
    case GEX_SAG_ERR_HEADER:
        return "header";
    case GEX_SAG_ERR_SENTINEL:
        return "sentinel";
    case GEX_SAG_ERR_TRUNCATED:
        return "truncated";
    case GEX_SAG_ERR_NO_DATA:
        return "no-data";
    }
    return "unknown";
}

gex_sag_status gex_sag_load(const char *path, unsigned char **data, size_t *size)
{
    if (path == NULL || data == NULL || size == NULL) {
        return GEX_SAG_ERR_ARGUMENT;
    }
    *data = NULL;
    *size = 0;

    FILE *file = fopen(path, "rb");
    if (file == NULL) {
        return GEX_SAG_ERR_IO;
    }
    if (fseek(file, 0, SEEK_END) != 0) {
        fclose(file);
        return GEX_SAG_ERR_IO;
    }
    const long length = ftell(file);
    if (length < 0 || fseek(file, 0, SEEK_SET) != 0) {
        fclose(file);
        return GEX_SAG_ERR_IO;
    }

    const size_t wanted = (size_t)length;
    if ((long)wanted != length) {
        fclose(file);
        return GEX_SAG_ERR_IO;
    }
    if (wanted == 0) {
        /* The original would allocate 0 bytes and fail on the first read. */
        fclose(file);
        return GEX_SAG_ERR_TRUNCATED;
    }

    unsigned char *buffer = (unsigned char *)malloc(wanted);
    if (buffer == NULL) {
        fclose(file);
        return GEX_SAG_ERR_IO;
    }
    const size_t got = fread(buffer, 1, wanted, file);
    fclose(file);
    if (got != wanted) {
        free(buffer);
        return GEX_SAG_ERR_IO;
    }

    *data = buffer;
    *size = wanted;
    return GEX_SAG_OK;
}

void gex_sag_release(void *data)
{
    free(data);
}
