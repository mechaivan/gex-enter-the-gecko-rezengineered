/*
 * vfx_texture.c - banco de texturas decodificadas de un `.VFX` (portable).
 *
 * Véase gex/vfx_texture.h. C11 puro: sin Windows, sin D3D11, sin Win32.
 */

#include "gex/vfx_texture.h"

#include <stdlib.h>
#include <string.h>

#define GEX_VFX_SLOT_NONE UINT32_MAX

static void bank_zero(gex_vfx_texture_bank *bank)
{
    memset(bank, 0, sizeof(*bank));
}

void gex_vfx_texture_bank_free(gex_vfx_texture_bank *bank)
{
    uint32_t i;

    if (bank == NULL) {
        return;
    }
    if (bank->textures != NULL) {
        for (i = 0; i < bank->texture_count; i++) {
            free(bank->textures[i].rgba);
        }
        free(bank->textures);
    }
    free(bank->slots);
    if (bank->vfx_loaded) {
        gex_vfx_close(&bank->vfx);
    }
    bank_zero(bank);
}

/* Decodifica una entrada del `.VFX` a RGBA8. Devuelve 1 si se pudo. */
static int decode_entry(const gex_vfx_record *rec, gex_vfx_texture *out)
{
    gex_vfx_texture_info info;
    gex_vfx_ncc_table ncc;
    gex_vfx_decode_status st;
    size_t bytes;

    if (!gex_vfx_texture_info_get(rec, &info)) {
        return 0;
    }
    if (info.levels != 1u) {
        /* Solo se implementa el nivel único (es el caso de los 15 251
         * registros medidos). Un nivel con cadena de mipmap no se decodifica
         * a medias. */
        return 0;
    }
    if (rec->data == NULL || rec->data_size == 0u) {
        return 0;
    }
    if (info.width == 0u || info.height == 0u) {
        return 0;
    }
    if ((size_t)info.width * (size_t)info.height >
        SIZE_MAX / 4u) {
        return 0;
    }
    bytes = (size_t)info.width * (size_t)info.height * 4u;
    out->rgba = (uint8_t *)calloc(bytes, 1);
    if (out->rgba == NULL) {
        return 0;
    }

    switch (rec->format) {
    case GEX_VFX_FORMAT_ARGB_1555:
        st = gex_vfx_decode_argb1555(rec->data, rec->data_size, info.width,
                                    info.height, out->rgba, bytes);
        break;
    case GEX_VFX_FORMAT_ARGB_4444:
        st = gex_vfx_decode_argb4444(rec->data, rec->data_size, info.width,
                                    info.height, out->rgba, bytes);
        break;
    case GEX_VFX_FORMAT_YIQ_422:
        if (rec->ncc_table == NULL ||
            rec->ncc_table_size < GEX_VFX_NCC_TABLE_BYTES) {
            free(out->rgba);
            out->rgba = NULL;
            return 0;
        }
        if (!gex_vfx_ncc_table_parse(rec->ncc_table, rec->ncc_table_size,
                                     &ncc)) {
            free(out->rgba);
            out->rgba = NULL;
            return 0;
        }
        st = gex_vfx_decode_yiq422(rec->data, rec->data_size, &ncc, info.width,
                                   info.height, out->rgba, bytes);
        break;
    default:
        free(out->rgba);
        out->rgba = NULL;
        return 0;
    }

    if (st != GEX_VFX_DEC_OK) {
        free(out->rgba);
        out->rgba = NULL;
        return 0;
    }
    out->format = rec->format;
    out->width = info.width;
    out->height = info.height;
    return 1;
}

gex_vfx_status gex_vfx_texture_bank_load(gex_vfx_texture_bank *bank,
                                         const char *vfx_path,
                                         const uint32_t *wanted,
                                         uint32_t wanted_count)
{
    gex_vfx_status st;
    uint32_t i;
    uint32_t slot;
    uint32_t max_index = 0;

    if (bank == NULL || vfx_path == NULL) {
        return GEX_VFX_ERR_ARG;
    }
    if (wanted == NULL && wanted_count > 0u) {
        return GEX_VFX_ERR_ARG;
    }
    bank_zero(bank);

    st = gex_vfx_open(&bank->vfx, vfx_path);
    if (st != GEX_VFX_OK) {
        return st;
    }
    bank->vfx_loaded = 1;

    if (wanted_count == 0u) {
        return GEX_VFX_OK;
    }

    for (i = 0; i < wanted_count; i++) {
        if (wanted[i] > max_index) {
            max_index = wanted[i];
        }
    }
    bank->slot_count = max_index + 1u;
    bank->slots = (uint32_t *)malloc((size_t)bank->slot_count *
                                     sizeof(*bank->slots));
    if (bank->slots == NULL) {
        gex_vfx_texture_bank_free(bank);
        return GEX_VFX_ERR_NOMEM;
    }
    for (i = 0; i < bank->slot_count; i++) {
        bank->slots[i] = GEX_VFX_SLOT_NONE;
    }
    bank->textures = (gex_vfx_texture *)calloc(wanted_count,
                                               sizeof(*bank->textures));
    if (bank->textures == NULL) {
        gex_vfx_texture_bank_free(bank);
        return GEX_VFX_ERR_NOMEM;
    }

    for (i = 0; i < wanted_count; i++) {
        uint32_t index = wanted[i];
        gex_vfx_texture tex;

        if (index >= (uint32_t)bank->vfx.record_count) {
            bank->failed_count++;
            continue;
        }
        if (bank->slots[index] != GEX_VFX_SLOT_NONE) {
            continue; /* ya pedida */
        }
        memset(&tex, 0, sizeof(tex));
        tex.index = index;
        if (!decode_entry(&bank->vfx.records[index], &tex)) {
            bank->failed_count++;
            continue;
        }
        slot = bank->texture_count;
        bank->textures[slot] = tex;
        bank->slots[index] = slot;
        bank->texture_count++;
        bank->decoded_count++;
        if (tex.width > bank->max_width) {
            bank->max_width = tex.width;
        }
        if (tex.height > bank->max_height) {
            bank->max_height = tex.height;
        }
    }

    return GEX_VFX_OK;
}

const gex_vfx_texture *gex_vfx_texture_bank_find(
    const gex_vfx_texture_bank *bank, uint32_t index)
{
    int slot;

    if (bank == NULL || bank->slots == NULL) {
        return NULL;
    }
    if (index >= bank->slot_count) {
        return NULL;
    }
    slot = gex_vfx_texture_bank_slot(bank, index);
    if (slot < 0) {
        return NULL;
    }
    return &bank->textures[(uint32_t)slot];
}

int gex_vfx_texture_bank_slot(const gex_vfx_texture_bank *bank, uint32_t index)
{
    if (bank == NULL || bank->slots == NULL) {
        return -1;
    }
    if (index >= bank->slot_count) {
        return -1;
    }
    if (bank->slots[index] == GEX_VFX_SLOT_NONE) {
        return -1;
    }
    return (int)bank->slots[index];
}
