/*
 * test_sag.c - checks for the SLAG reader. No framework, no dependencies:
 * exit 0 means every check passed.
 *
 * The fixtures are synthetic. No .sag file of the retail game is required (and
 * none is committed). The cases encode the accept/reject rules transcribed from
 * 0x0053899C; see docs/SAG_CONTAINER.md §4 and §6.
 */

#include "gex/asset/sag.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static int checks;
static int failures;

#define CHECK(cond)                                                          \
    do {                                                                     \
        checks++;                                                            \
        if (!(cond)) {                                                       \
            failures++;                                                      \
            printf("FAIL %s:%d: %s\n", __FILE__, __LINE__, #cond);           \
        }                                                                    \
    } while (0)

typedef struct {
    unsigned char bytes[1024];
    size_t size;
} fixture;

static uint16_t le16(const unsigned char *p)
{
    return (uint16_t)((uint16_t)p[0] | ((uint16_t)p[1] << 8));
}

static uint32_t le32(const unsigned char *p)
{
    return (uint32_t)p[0] | ((uint32_t)p[1] << 8) | ((uint32_t)p[2] << 16) |
           ((uint32_t)p[3] << 24);
}

static void put_u16(fixture *f, uint16_t value)
{
    f->bytes[f->size++] = (unsigned char)(value & 0xFF);
    f->bytes[f->size++] = (unsigned char)((value >> 8) & 0xFF);
}

static void put_u32(fixture *f, uint32_t value)
{
    f->bytes[f->size++] = (unsigned char)(value & 0xFF);
    f->bytes[f->size++] = (unsigned char)((value >> 8) & 0xFF);
    f->bytes[f->size++] = (unsigned char)((value >> 16) & 0xFF);
    f->bytes[f->size++] = (unsigned char)((value >> 24) & 0xFF);
}

static void put_tag(fixture *f, const char tag[4])
{
    memcpy(f->bytes + f->size, tag, 4);
    f->size += 4;
}

static void put_fill(fixture *f, size_t count, unsigned char value)
{
    memset(f->bytes + f->size, value, count);
    f->size += count;
}

static void put_wfx(fixture *f, uint16_t format_tag, uint16_t channels,
                    uint32_t rate, uint32_t avg, uint16_t align,
                    uint16_t bits, uint16_t cb_size)
{
    put_u16(f, format_tag);
    put_u16(f, channels);
    put_u32(f, rate);
    put_u32(f, avg);
    put_u16(f, align);
    put_u16(f, bits);
    put_u16(f, cb_size);
}

static void put_header(fixture *f, uint32_t form_size)
{
    f->size = 0;
    put_tag(f, "SLAG");
    put_u32(f, form_size);
    put_tag(f, "WAVE");
}

static void put_valid(fixture *f, uint32_t data_size)
{
    put_header(f, 0x1A);
    put_tag(f, "fmt ");
    put_u32(f, 18);
    put_wfx(f, 1, 1, 22050, 22050, 1, 8, 0);
    put_tag(f, "data");
    put_u32(f, data_size);
    put_fill(f, data_size, 0x80);
}

static void test_valid_basic(void)
{
    fixture f;
    gex_sag sag;
    put_valid(&f, 8);

    CHECK(gex_sag_parse(f.bytes, f.size, &sag) == GEX_SAG_OK);
    CHECK(sag.form_size == 0x1A);
    CHECK(sag.has_format == 1);
    CHECK(sag.format_chunk_size == 18);
    CHECK(sag.format_bytes == 18);
    CHECK(le16(sag.format + 0) == 1);
    CHECK(le16(sag.format + 2) == 1);
    CHECK(le32(sag.format + 4) == 22050);
    CHECK(le32(sag.format + 8) == 22050);
    CHECK(le16(sag.format + 12) == 1);
    CHECK(le16(sag.format + 14) == 8);
    CHECK(le16(sag.format + 16) == 0);
    CHECK(sag.data_size == 8);
    CHECK(sag.data_offset == 12 + 8 + 18 + 8);
    CHECK(sag.data_complete == 1);
    CHECK(sag.data_offset + sag.data_size <= f.size);
}

static void test_unknown_chunk_before_fmt(void)
{
    fixture f;
    gex_sag sag;
    put_header(&f, 0x20);
    put_tag(&f, "junk");
    put_u32(&f, 3);
    put_fill(&f, 3, 0x11);
    put_tag(&f, "fmt ");
    put_u32(&f, 18);
    put_wfx(&f, 1, 2, 44100, 176400, 4, 16, 0);
    put_tag(&f, "data");
    put_u32(&f, 4);
    put_fill(&f, 4, 0);

    CHECK(gex_sag_parse(f.bytes, f.size, &sag) == GEX_SAG_OK);
    CHECK(sag.has_format == 1);
    CHECK(le16(sag.format + 2) == 2);
    CHECK(sag.data_size == 4);
    CHECK(sag.data_offset == 12 + (8 + 3) + (8 + 18) + 8);
}

static void test_fmt_smaller_than_18(void)
{
    fixture f;
    gex_sag sag;
    put_header(&f, 0x10);
    put_tag(&f, "fmt ");
    put_u32(&f, 16);
    /* WAVEFORMAT (16 bytes): no cbSize field, as some WAV writers emit. */
    put_u16(&f, 1);
    put_u16(&f, 1);
    put_u32(&f, 11025);
    put_u32(&f, 11025);
    put_u16(&f, 1);
    put_u16(&f, 8);
    put_tag(&f, "data");
    put_u32(&f, 0);

    CHECK(gex_sag_parse(f.bytes, f.size, &sag) == GEX_SAG_OK);
    CHECK(sag.format_bytes == 16);
    CHECK(sag.format_chunk_size == 16);
    CHECK(sag.format[16] == 0);
    CHECK(sag.format[17] == 0);
    CHECK(sag.data_offset == 12 + 8 + 16 + 8);
}

static void test_fmt_larger_than_18(void)
{
    fixture f;
    gex_sag sag;
    put_header(&f, 0x20);
    put_tag(&f, "fmt ");
    put_u32(&f, 20);
    put_wfx(&f, 1, 1, 8000, 8000, 1, 8, 0);
    put_fill(&f, 2, 0xEE); /* extra bytes after WAVEFORMATEX */
    put_tag(&f, "data");
    put_u32(&f, 1);
    put_fill(&f, 1, 0);

    CHECK(gex_sag_parse(f.bytes, f.size, &sag) == GEX_SAG_OK);
    CHECK(sag.format_bytes == 18);
    CHECK(sag.format_chunk_size == 20);
    CHECK(le32(sag.format + 4) == 8000);
    CHECK(sag.data_offset == 12 + 8 + 20 + 8);
}

static void test_last_fmt_wins(void)
{
    fixture f;
    gex_sag sag;
    put_header(&f, 0x30);
    put_tag(&f, "fmt ");
    put_u32(&f, 18);
    put_wfx(&f, 1, 1, 22050, 22050, 1, 8, 0);
    put_tag(&f, "fmt ");
    put_u32(&f, 18);
    put_wfx(&f, 2, 2, 44100, 176400, 4, 16, 0);
    put_tag(&f, "data");
    put_u32(&f, 0);

    CHECK(gex_sag_parse(f.bytes, f.size, &sag) == GEX_SAG_OK);
    CHECK(le16(sag.format + 0) == 2);
    CHECK(le16(sag.format + 2) == 2);
    CHECK(sag.format_chunk_size == 18);
}

static void test_data_before_fmt(void)
{
    fixture f;
    gex_sag sag;
    put_header(&f, 0x14);
    put_tag(&f, "data");
    put_u32(&f, 0);
    put_tag(&f, "fmt ");
    put_u32(&f, 18);
    put_wfx(&f, 1, 1, 22050, 22050, 1, 8, 0);

    CHECK(gex_sag_parse(f.bytes, f.size, &sag) == GEX_SAG_OK);
    CHECK(sag.has_format == 0);
    CHECK(sag.data_size == 0);
    CHECK(sag.data_offset == 20);
    CHECK(sag.data_complete == 1);
}

static void test_zero_length_data(void)
{
    fixture f;
    gex_sag sag;
    put_valid(&f, 0);

    CHECK(gex_sag_parse(f.bytes, f.size, &sag) == GEX_SAG_OK);
    CHECK(sag.data_size == 0);
    CHECK(sag.data_offset == f.size);
    CHECK(sag.data_complete == 1);
}

static void test_truncated_data_payload(void)
{
    fixture f;
    gex_sag sag;
    put_header(&f, 0x10);
    put_tag(&f, "data");
    put_u32(&f, 8);
    put_fill(&f, 3, 0);

    CHECK(gex_sag_parse(f.bytes, f.size, &sag) == GEX_SAG_OK);
    CHECK(sag.data_size == 8);
    CHECK(sag.data_complete == 0);
    CHECK(sag.data_offset + 3 == f.size);
}

static void test_missing_slag(void)
{
    fixture f;
    gex_sag sag;
    put_valid(&f, 0);
    memcpy(f.bytes, "SLAR", 4);

    CHECK(gex_sag_parse(f.bytes, f.size, &sag) == GEX_SAG_ERR_HEADER);
    CHECK(sag.data_size == 0);
    CHECK(sag.has_format == 0);
}

static void test_sentinel_form_size(void)
{
    fixture f;
    gex_sag sag;
    put_valid(&f, 0);
    f.bytes[4] = 0xFF;
    f.bytes[5] = 0xFF;
    f.bytes[6] = 0xFF;
    f.bytes[7] = 0xFF;

    CHECK(gex_sag_parse(f.bytes, f.size, &sag) == GEX_SAG_ERR_SENTINEL);
}

static void test_missing_wave(void)
{
    fixture f;
    gex_sag sag;
    put_valid(&f, 0);
    memcpy(f.bytes + 8, "WAVX", 4);

    CHECK(gex_sag_parse(f.bytes, f.size, &sag) == GEX_SAG_ERR_HEADER);
}

static void test_short_header(void)
{
    fixture f;
    gex_sag sag;
    put_valid(&f, 0);

    CHECK(gex_sag_parse(f.bytes, 11, &sag) == GEX_SAG_ERR_TRUNCATED);
    CHECK(gex_sag_parse(f.bytes, 0, &sag) == GEX_SAG_ERR_TRUNCATED);
}

static void test_short_chunk_header(void)
{
    fixture f;
    gex_sag sag;
    put_header(&f, 0);
    put_tag(&f, "fmt ");

    CHECK(gex_sag_parse(f.bytes, f.size, &sag) == GEX_SAG_ERR_TRUNCATED);
}

static void test_fmt_payload_truncated(void)
{
    fixture f;
    gex_sag sag;
    put_header(&f, 0);
    put_tag(&f, "fmt ");
    put_u32(&f, 18);
    put_fill(&f, 10, 0);

    CHECK(gex_sag_parse(f.bytes, f.size, &sag) == GEX_SAG_ERR_TRUNCATED);
}

static void test_no_data_chunk(void)
{
    fixture f;
    gex_sag sag;
    put_header(&f, 0);
    put_tag(&f, "junk");
    put_u32(&f, 4);
    put_fill(&f, 4, 0);

    CHECK(gex_sag_parse(f.bytes, f.size, &sag) == GEX_SAG_ERR_NO_DATA);
}

static void test_chunk_past_end(void)
{
    fixture f;
    gex_sag sag;
    put_header(&f, 0);
    put_tag(&f, "junk");
    put_u32(&f, 100);
    put_fill(&f, 3, 0);

    CHECK(gex_sag_parse(f.bytes, f.size, &sag) == GEX_SAG_ERR_TRUNCATED);
}

static void test_chunk_size_overflow(void)
{
    fixture f;
    gex_sag sag;
    put_header(&f, 0);
    put_tag(&f, "junk");
    put_u32(&f, 0xFFFFFFFEu);
    put_fill(&f, 3, 0);

    CHECK(gex_sag_parse(f.bytes, f.size, &sag) == GEX_SAG_ERR_TRUNCATED);
}

static void test_sentinel_tag_and_size(void)
{
    fixture f;
    gex_sag sag;
    put_header(&f, 0);
    put_tag(&f, "junk");
    put_u32(&f, 0xFFFFFFFFu);

    CHECK(gex_sag_parse(f.bytes, f.size, &sag) == GEX_SAG_ERR_SENTINEL);

    put_header(&f, 0);
    put_u32(&f, 0xFFFFFFFFu);
    put_u32(&f, 0);
    CHECK(gex_sag_parse(f.bytes, f.size, &sag) == GEX_SAG_ERR_SENTINEL);
}

static void test_zero_length_fmt(void)
{
    fixture f;
    gex_sag sag;
    put_header(&f, 0);
    put_tag(&f, "fmt ");
    put_u32(&f, 0);
    put_tag(&f, "data");
    put_u32(&f, 0);

    CHECK(gex_sag_parse(f.bytes, f.size, &sag) == GEX_SAG_OK);
    CHECK(sag.has_format == 1);
    CHECK(sag.format_bytes == 0);
    CHECK(sag.format_chunk_size == 0);
    CHECK(sag.format[0] == 0 && sag.format[17] == 0);
    CHECK(sag.data_size == 0);
}

static void test_zero_length_fmt_at_eof(void)
{
    fixture f;
    gex_sag sag;
    put_header(&f, 0);
    put_tag(&f, "fmt ");
    put_u32(&f, 0);

    CHECK(gex_sag_parse(f.bytes, f.size, &sag) == GEX_SAG_ERR_NO_DATA);
}

static void test_max_data_size(void)
{
    fixture f;
    gex_sag sag;
    put_header(&f, 0);
    put_tag(&f, "data");
    put_u32(&f, 0xFFFFFFFEu);

    CHECK(gex_sag_parse(f.bytes, f.size, &sag) == GEX_SAG_OK);
    CHECK(sag.data_size == 0xFFFFFFFEu);
    CHECK(sag.data_complete == 0);
}

static void test_form_size_value(void)
{
    fixture f;
    gex_sag sag;
    put_header(&f, 0x01020304u);
    put_tag(&f, "data");
    put_u32(&f, 0);

    CHECK(gex_sag_parse(f.bytes, f.size, &sag) == GEX_SAG_OK);
    CHECK(sag.form_size == 0x01020304u);
}

static void test_fmt_zero_padding(void)
{
    fixture f;
    gex_sag sag;
    put_header(&f, 0);
    put_tag(&f, "fmt ");
    put_u32(&f, 18);
    put_fill(&f, 18, 0xAA);
    put_tag(&f, "fmt ");
    put_u32(&f, 4);
    put_fill(&f, 4, 0x01);
    put_tag(&f, "data");
    put_u32(&f, 0);

    CHECK(gex_sag_parse(f.bytes, f.size, &sag) == GEX_SAG_OK);
    CHECK(sag.format_bytes == 4);
    CHECK(sag.format[0] == 0x01 && sag.format[3] == 0x01);
    CHECK(sag.format[4] == 0 && sag.format[17] == 0);
}

static void test_argument_validation(void)
{
    fixture f;
    gex_sag sag;
    unsigned char *data = (unsigned char *)1;
    size_t size = 1;
    put_valid(&f, 0);

    CHECK(gex_sag_parse(NULL, 10, &sag) == GEX_SAG_ERR_ARGUMENT);
    CHECK(gex_sag_parse(f.bytes, f.size, NULL) == GEX_SAG_ERR_ARGUMENT);
    CHECK(gex_sag_load(NULL, &data, &size) == GEX_SAG_ERR_ARGUMENT);
    CHECK(gex_sag_load("x.sag", NULL, &size) == GEX_SAG_ERR_ARGUMENT);
    CHECK(gex_sag_load("x.sag", &data, NULL) == GEX_SAG_ERR_ARGUMENT);
}

static void test_status_names(void)
{
    const gex_sag_status all[] = {
        GEX_SAG_OK,          GEX_SAG_ERR_ARGUMENT, GEX_SAG_ERR_IO,
        GEX_SAG_ERR_HEADER,  GEX_SAG_ERR_SENTINEL, GEX_SAG_ERR_TRUNCATED,
        GEX_SAG_ERR_NO_DATA,
    };
    for (size_t i = 0; i < sizeof(all) / sizeof(all[0]); i++) {
        const char *name = gex_sag_status_name(all[i]);
        CHECK(name != NULL);
        CHECK(name[0] != '\0');
        for (size_t j = 0; j < i; j++) {
            CHECK(strcmp(name, gex_sag_status_name(all[j])) != 0);
        }
    }
    CHECK(strcmp(gex_sag_status_name((gex_sag_status)99), "unknown") == 0);
}

static void test_load_file(void)
{
    const char *path = "test_sag_tmp.sag";
    fixture f;
    gex_sag sag;
    unsigned char *data = NULL;
    size_t size = 0;

    put_valid(&f, 12);
    FILE *file = fopen(path, "wb");
    CHECK(file != NULL);
    if (file != NULL) {
        CHECK(fwrite(f.bytes, 1, f.size, file) == f.size);
        CHECK(fclose(file) == 0);
    }

    CHECK(gex_sag_load(path, &data, &size) == GEX_SAG_OK);
    CHECK(data != NULL);
    CHECK(size == f.size);
    if (data != NULL) {
        CHECK(gex_sag_parse(data, size, &sag) == GEX_SAG_OK);
        CHECK(sag.data_size == 12);
        CHECK(sag.data_offset + 12 == size);
    }
    gex_sag_release(data);
    remove(path);
}

static void test_load_errors(void)
{
    unsigned char *data = (unsigned char *)1;
    size_t size = 1;
    const char *path = "test_sag_empty_tmp.sag";

    CHECK(gex_sag_load("test_sag_missing_9f3c.sag", &data, &size) == GEX_SAG_ERR_IO);
    CHECK(data == NULL);
    CHECK(size == 0);

    FILE *file = fopen(path, "wb");
    CHECK(file != NULL);
    if (file != NULL) {
        CHECK(fclose(file) == 0);
    }
    CHECK(gex_sag_load(path, &data, &size) == GEX_SAG_ERR_TRUNCATED);
    CHECK(data == NULL);
    remove(path);

    gex_sag_release(NULL); /* must not crash */
}

static uint32_t rng_state = 0x12345678u;

static uint32_t next_random(void)
{
    uint32_t x = rng_state;
    x ^= x << 13;
    x ^= x >> 17;
    x ^= x << 5;
    rng_state = x;
    return x;
}

static void check_invariants(const gex_sag *sag, size_t size, int parsed)
{
    if (!parsed) {
        return;
    }
    CHECK(sag->data_offset <= size);
    if (sag->data_complete) {
        CHECK(sag->data_size <= size - sag->data_offset);
    }
    CHECK(sag->format_bytes <= GEX_SAG_FORMAT_BYTES);
    if (sag->has_format) {
        const uint32_t limit = sag->format_chunk_size < GEX_SAG_FORMAT_BYTES
                                   ? sag->format_chunk_size
                                   : GEX_SAG_FORMAT_BYTES;
        CHECK(sag->format_bytes == (size_t)limit);
    }
}

static void test_fuzz_random(void)
{
    unsigned char buffer[512];
    int accepted = 0;

    for (int i = 0; i < 20000; i++) {
        const size_t size = next_random() % sizeof(buffer);
        for (size_t j = 0; j < size; j++) {
            buffer[j] = (unsigned char)(next_random() & 0xFF);
        }
        gex_sag sag;
        const gex_sag_status status = gex_sag_parse(buffer, size, &sag);
        CHECK(status >= GEX_SAG_OK && status <= GEX_SAG_ERR_NO_DATA);
        if (status == GEX_SAG_OK) {
            accepted++;
        }
        check_invariants(&sag, size, status == GEX_SAG_OK);
    }
    /* A random buffer may legitimately pass; the point is no crash and no
     * out-of-range field. Keep the counter visible for the log. */
    printf("fuzz: 20000 random buffers, %d accepted\n", accepted);
}

static void test_fuzz_mutations(void)
{
    fixture base;
    put_valid(&base, 16);

    for (int i = 0; i < 20000; i++) {
        fixture f = base;
        const int flips = 1 + (int)(next_random() % 4);
        for (int k = 0; k < flips; k++) {
            const size_t at = next_random() % f.size;
            f.bytes[at] ^= (unsigned char)(1u << (next_random() % 8));
        }
        const size_t size = f.size - (next_random() % 8);
        gex_sag sag;
        const gex_sag_status status = gex_sag_parse(f.bytes, size, &sag);
        CHECK(status >= GEX_SAG_OK && status <= GEX_SAG_ERR_NO_DATA);
        check_invariants(&sag, size, status == GEX_SAG_OK);
    }
}

int main(void)
{
    test_valid_basic();
    test_unknown_chunk_before_fmt();
    test_fmt_smaller_than_18();
    test_fmt_larger_than_18();
    test_last_fmt_wins();
    test_data_before_fmt();
    test_zero_length_data();
    test_truncated_data_payload();
    test_missing_slag();
    test_sentinel_form_size();
    test_missing_wave();
    test_short_header();
    test_short_chunk_header();
    test_fmt_payload_truncated();
    test_no_data_chunk();
    test_chunk_past_end();
    test_chunk_size_overflow();
    test_sentinel_tag_and_size();
    test_zero_length_fmt();
    test_zero_length_fmt_at_eof();
    test_max_data_size();
    test_form_size_value();
    test_fmt_zero_padding();
    test_argument_validation();
    test_status_names();
    test_load_file();
    test_load_errors();
    test_fuzz_random();
    test_fuzz_mutations();

    printf("%d checks, %d failures\n", checks, failures);
    return failures == 0 ? 0 : 1;
}
