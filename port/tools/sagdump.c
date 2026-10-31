/*
 * sagdump - print what the native SLAG reader understood from a file.
 *
 * This is the visible check for the first port component: it loads the file the
 * same way 0x005388AC does and prints the header fields, the WAVEFORMATEX from
 * the "fmt " chunk and the "data" offset/size.
 *
 * It does not play audio and does not validate that the PCM payload is correct.
 */

#include "gex/asset/sag.h"

#include <stdio.h>

static uint16_t le16(const unsigned char *p)
{
    return (uint16_t)((uint16_t)p[0] | ((uint16_t)p[1] << 8));
}

static uint32_t le32(const unsigned char *p)
{
    return (uint32_t)p[0] | ((uint32_t)p[1] << 8) | ((uint32_t)p[2] << 16) |
           ((uint32_t)p[3] << 24);
}

int main(int argc, char **argv)
{
    if (argc != 2) {
        fprintf(stderr, "usage: sagdump <file.sag>\n");
        return 2;
    }

    unsigned char *data = NULL;
    size_t size = 0;
    gex_sag_status status = gex_sag_load(argv[1], &data, &size);
    if (status != GEX_SAG_OK) {
        fprintf(stderr, "%s: load failed: %s\n", argv[1],
                gex_sag_status_name(status));
        return 1;
    }

    gex_sag sag;
    status = gex_sag_parse(data, size, &sag);
    printf("file:   %s\n", argv[1]);
    printf("size:   %zu bytes\n", size);
    if (status != GEX_SAG_OK) {
        printf("parse:  %s\n", gex_sag_status_name(status));
        gex_sag_release(data);
        return 1;
    }

    printf("form:   SLAG size=0x%08X (%u)\n", sag.form_size, sag.form_size);
    if (sag.has_format) {
        printf("format: chunk=%u copied=%zu\n", sag.format_chunk_size,
               sag.format_bytes);
        printf("        wFormatTag=0x%04X nChannels=%u nSamplesPerSec=%u\n",
               le16(sag.format + 0), le16(sag.format + 2),
               le32(sag.format + 4));
        printf("        nAvgBytesPerSec=%u nBlockAlign=%u wBitsPerSample=%u "
               "cbSize=%u\n",
               le32(sag.format + 8), le16(sag.format + 12),
               le16(sag.format + 14), le16(sag.format + 16));
    } else {
        printf("format: none (no \"fmt \" chunk before \"data\")\n");
    }
    printf("data:   offset=%zu size=%u %s\n", sag.data_offset, sag.data_size,
           sag.data_complete ? "complete" : "TRUNCATED");

    gex_sag_release(data);
    return 0;
}
