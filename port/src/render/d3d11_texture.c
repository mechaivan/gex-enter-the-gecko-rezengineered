/*
 * d3d11_texture.c - atlas de texturas para Direct3D 11 (Módulo 4C,
 * Windows-only).
 *
 * Véase gex/render/d3d11_texture.h para la justificación de la estrategia
 * (atlas y no texture array).
 */

#include "gex/render/d3d11_texture.h"

#if defined(_WIN32)

#include <stdlib.h>
#include <string.h>

/* Constant buffer del shader: cabecera + una entrada de 32 B por textura.
 * La entrada son dos float4 para respetar la alineación de 16 B de los
 * constant buffers de HLSL. */
typedef struct {
    float header[4]; /* x = nº de entradas, y = ancho del atlas,
                      * z = alto del atlas, w = modo de UV */
    float entry[GEX_D3D11_TEXTURE_MAX_ENTRIES][8];
} gex_d3d11_tex_constants;

/* Una textura dentro del atlas: origen (con el borde) y tamaño. */
typedef struct {
    UINT x, y;      /* esquina del contenido (ya dentro del borde) */
    UINT w, h;      /* tamaño original */
} atlas_slot;

/*
 * Coloca las texturas en el atlas: una fila por textura mientras quepan en
 * el ancho máximo; si no, se empieza una fila nueva. Cada textura ocupa
 * (w+2) x (h+2): un texel de borde replicado para que el filtrado bilineal
 * no mezcle con la vecina.
 */
static void atlas_layout(const gex_vfx_texture_bank *bank,
                         atlas_slot *slots, UINT *width, UINT *height)
{
    UINT x = 0;
    UINT y = 0;
    UINT row_height = 0;
    UINT i;

    for (i = 0; i < bank->texture_count; i++) {
        const gex_vfx_texture *tex = &bank->textures[i];
        UINT tw = tex->width + 2u;
        UINT th = tex->height + 2u;

        if (x + tw > GEX_D3D11_TEXTURE_ATLAS_MAX_WIDTH && x > 0u) {
            x = 0u;
            y += row_height;
            row_height = 0u;
        }
        slots[i].x = x + 1u;
        slots[i].y = y + 1u;
        slots[i].w = tex->width;
        slots[i].h = tex->height;
        x += tw;
        if (th > row_height) {
            row_height = th;
        }
    }
    *width = x;
    *height = y + row_height;
    if (*width == 0u) {
        *width = 1u;
    }
    if (*height == 0u) {
        *height = 1u;
    }
}

/*
 * Copia una textura en el atlas. La celda ocupa (w+2) x (h+2): el contenido
 * en (x, y) y un texel de borde alrededor con el color del borde repetido,
 * para que el filtrado bilineal no mezcle una textura con la vecina.
 */
static void blit_with_border(uint8_t *atlas, UINT atlas_width,
                             const atlas_slot *slot, const uint8_t *rgba)
{
    UINT x;
    UINT y;
    uint8_t *cell;

    if (rgba == NULL) {
        return;
    }
    cell = atlas + ((size_t)slot->y * atlas_width + (size_t)slot->x) * 4u;

    /* Contenido. */
    for (y = 0; y < slot->h; y++) {
        memcpy(cell + (size_t)y * (size_t)atlas_width * 4u,
               rgba + (size_t)y * (size_t)slot->w * 4u,
               (size_t)slot->w * 4u);
    }

    /* Borde superior: se replica la primera fila del contenido. */
    for (x = 0; x < slot->w; x++) {
        memcpy(cell - (size_t)atlas_width * 4u + (size_t)x * 4u,
               rgba + (size_t)x * 4u, 4u);
        /* Borde inferior: la última fila. */
        memcpy(cell + (size_t)slot->h * (size_t)atlas_width * 4u +
                   (size_t)x * 4u,
               rgba + ((size_t)slot->h - 1u) * (size_t)slot->w * 4u +
                   (size_t)x * 4u,
               4u);
    }
    /* Bordes laterales: la primera y la última columna. */
    for (y = 0; y < slot->h; y++) {
        const uint8_t *src = rgba + (size_t)y * (size_t)slot->w * 4u;
        uint8_t *row = cell + (size_t)y * (size_t)atlas_width * 4u;
        memcpy(row - 4u, src, 4u);
        memcpy(row + (size_t)slot->w * 4u, src + ((size_t)slot->w - 1u) * 4u,
               4u);
    }
    /* Esquinas. */
    memcpy(cell - (size_t)atlas_width * 4u - 4u, rgba, 4u);
    memcpy(cell - (size_t)atlas_width * 4u + (size_t)slot->w * 4u,
           rgba + ((size_t)slot->w - 1u) * 4u, 4u);
    memcpy(cell + (size_t)slot->h * (size_t)atlas_width * 4u - 4u,
           rgba + ((size_t)slot->h - 1u) * (size_t)slot->w * 4u, 4u);
    memcpy(cell + (size_t)slot->h * (size_t)atlas_width * 4u +
               (size_t)slot->w * 4u,
           rgba + ((size_t)slot->h - 1u) * (size_t)slot->w * 4u +
               ((size_t)slot->w - 1u) * 4u,
           4u);
}

gex_d3d11_texture_status gex_d3d11_texture_set_create(
    gex_d3d11_texture_set *set, const gex_d3d11_device *device,
    const gex_vfx_texture_bank *bank, gex_ncc_uv_mode uv_mode)
{
    D3D11_TEXTURE2D_DESC desc;
    D3D11_SAMPLER_DESC samp_desc;
    D3D11_BUFFER_DESC cb_desc;
    D3D11_SUBRESOURCE_DATA init;
    gex_d3d11_tex_constants constants;
    atlas_slot *slots = NULL;
    uint8_t *pixels = NULL;
    uint32_t i;
    uint32_t slice_index;
    UINT atlas_width;
    UINT atlas_height;
    HRESULT hr;

    if (set == NULL || device == NULL || bank == NULL) {
        return GEX_D3D11_TEXTURE_BAD_ARGUMENT;
    }
    if (set->initialized) {
        return GEX_D3D11_TEXTURE_BAD_STATE;
    }
    if (!device->initialized || device->device == NULL) {
        return GEX_D3D11_TEXTURE_BAD_ARGUMENT;
    }
    if (bank->texture_count == 0u) {
        return GEX_D3D11_TEXTURE_BAD_ARGUMENT;
    }
    if (bank->texture_count > GEX_D3D11_TEXTURE_MAX_ENTRIES) {
        return GEX_D3D11_TEXTURE_TOO_MANY;
    }

    memset(set, 0, sizeof(*set));
    set->last_hresult = S_OK;
    set->device = device->device;
    set->uv_mode = uv_mode;
    set->texture_count = bank->texture_count;

    /* Colocación en el atlas. */
    slots = (atlas_slot *)calloc(bank->texture_count, sizeof(*slots));
    if (slots == NULL) {
        return GEX_D3D11_TEXTURE_OUT_OF_MEMORY;
    }
    atlas_layout(bank, slots, &atlas_width, &atlas_height);
    set->atlas_width = atlas_width;
    set->atlas_height = atlas_height;

    /* Imagen del atlas: fondo transparente y las texturas en su sitio. */
    pixels = (uint8_t *)calloc((size_t)atlas_width * (size_t)atlas_height * 4u,
                               1);
    if (pixels == NULL) {
        free(slots);
        return GEX_D3D11_TEXTURE_OUT_OF_MEMORY;
    }
    for (i = 0; i < bank->texture_count; i++) {
        blit_with_border(pixels, atlas_width, &slots[i],
                         bank->textures[i].rgba);
    }

    memset(&desc, 0, sizeof(desc));
    desc.Width = atlas_width;
    desc.Height = atlas_height;
    desc.MipLevels = 1;
    desc.ArraySize = 1;
    desc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
    desc.SampleDesc.Count = 1;
    desc.SampleDesc.Quality = 0;
    desc.Usage = D3D11_USAGE_IMMUTABLE;
    desc.BindFlags = D3D11_BIND_SHADER_RESOURCE;
    desc.CPUAccessFlags = 0;
    desc.MiscFlags = 0;

    memset(&init, 0, sizeof(init));
    init.pSysMem = pixels;
    init.SysMemPitch = (UINT)(atlas_width * 4u);
    init.SysMemSlicePitch = 0;

    hr = ID3D11Device_CreateTexture2D(set->device, &desc, &init, &set->atlas);
    set->last_hresult = hr;
    free(pixels);
    if (FAILED(hr) || set->atlas == NULL) {
        set->atlas = NULL;
        free(slots);
        return GEX_D3D11_TEXTURE_CREATE_FAILED;
    }

    /* Shader resource view del atlas. */
    {
        D3D11_SHADER_RESOURCE_VIEW_DESC srv_desc;
        memset(&srv_desc, 0, sizeof(srv_desc));
        srv_desc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
        srv_desc.ViewDimension = D3D11_SRV_DIMENSION_TEXTURE2D;
        srv_desc.Texture2D.MostDetailedMip = 0;
        srv_desc.Texture2D.MipLevels = 1;
        hr = ID3D11Device_CreateShaderResourceView(
            set->device, (ID3D11Resource *)set->atlas, &srv_desc, &set->srv);
    }
    set->last_hresult = hr;
    if (FAILED(hr) || set->srv == NULL) {
        set->srv = NULL;
        ID3D11Texture2D_Release(set->atlas);
        set->atlas = NULL;
        free(slots);
        return GEX_D3D11_TEXTURE_CREATE_FAILED;
    }

    /* Sampler: lineal + wrap (los valores por defecto de Glide 2.x, que el
     * juego nunca cambia: no importa grTexFilterMode ni grTexClampMode). */
    memset(&samp_desc, 0, sizeof(samp_desc));
    samp_desc.Filter = D3D11_FILTER_MIN_MAG_MIP_LINEAR;
    samp_desc.AddressU = D3D11_TEXTURE_ADDRESS_WRAP;
    samp_desc.AddressV = D3D11_TEXTURE_ADDRESS_WRAP;
    samp_desc.AddressW = D3D11_TEXTURE_ADDRESS_WRAP;
    samp_desc.MipLODBias = 0.0f;
    samp_desc.MaxAnisotropy = 1;
    samp_desc.ComparisonFunc = D3D11_COMPARISON_NEVER;
    samp_desc.MinLOD = 0.0f;
    samp_desc.MaxLOD = D3D11_FLOAT32_MAX;

    hr = ID3D11Device_CreateSamplerState(set->device, &samp_desc,
                                         &set->sampler);
    set->last_hresult = hr;
    if (FAILED(hr) || set->sampler == NULL) {
        set->sampler = NULL;
        ID3D11ShaderResourceView_Release(set->srv);
        set->srv = NULL;
        ID3D11Texture2D_Release(set->atlas);
        set->atlas = NULL;
        free(slots);
        return GEX_D3D11_TEXTURE_CREATE_FAILED;
    }

    /* Tabla de búsqueda: índice del `.VFX` -> posición y tamaño en el
     * atlas. */
    memset(&constants, 0, sizeof(constants));
    constants.header[0] = (float)bank->texture_count;
    constants.header[1] = (float)atlas_width;
    constants.header[2] = (float)atlas_height;
    constants.header[3] = (float)uv_mode;

    slice_index = 0;
    for (i = 0; i < (uint32_t)bank->vfx.record_count; i++) {
        int slot;

        if (i >= bank->slot_count) {
            continue;
        }
        slot = gex_vfx_texture_bank_slot(bank, i);
        if (slot < 0) {
            continue;
        }
        /* La entrada describe la textura `slot` del banco. */
        constants.entry[slice_index][0] = (float)i;             /* índice */
        constants.entry[slice_index][1] = (float)slots[slot].w; /* ancho */
        constants.entry[slice_index][2] = (float)slots[slot].h; /* alto */
        constants.entry[slice_index][3] = (float)slots[slot].x; /* x */
        constants.entry[slice_index][4] = (float)slots[slot].y; /* y */
        /* Flag de canal alfa (0 = opaca, 1 = con alfa).
         *
         * El original decide el modo de mezcla POR POLIGONO a partir del
         * registro de textura en tiempo de ejecucion: en 0x46558A se compara
         * el campo +0x0C del registro con 0x0C (la textura trae alfa) y con
         * 0 (es opaca). Ese campo es el formato con el que el juego tiene la
         * textura: solo el ARGB_4444 conserva el canal alfa; el YIQ_422 y el
         * ARGB_1555 los convierte a un formato opaco de 8 bits.
         *
         * Comprobado en MAP7 con el original en ejecucion (2026-11-11): las
         * seis texturas que el original MEZCLA con alfa (grAlphaCombine
         * (3,4,1,0,0)) son exactamente las de formato ARGB_4444 del
         * `map7.vfx` (indices 6, 8, 185, 188, 234 y 241); las demas se
         * dibujan opacas (grAlphaCombine(1,0,0,2,0)). */
        constants.entry[slice_index][5] =
            (bank->vfx.records[i].format == GEX_VFX_FORMAT_ARGB_4444)
                ? 1.0f : 0.0f;
        slice_index++;
        if (slice_index >= GEX_D3D11_TEXTURE_MAX_ENTRIES) {
            break;
        }
    }
    set->entry_count = slice_index;
    free(slots);

    memset(&cb_desc, 0, sizeof(cb_desc));
    cb_desc.ByteWidth = (UINT)sizeof(constants);
    cb_desc.Usage = D3D11_USAGE_IMMUTABLE;
    cb_desc.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
    cb_desc.CPUAccessFlags = 0;
    cb_desc.MiscFlags = 0;
    cb_desc.StructureByteStride = 0;

    memset(&init, 0, sizeof(init));
    init.pSysMem = &constants;
    init.SysMemPitch = 0;
    init.SysMemSlicePitch = 0;

    hr = ID3D11Device_CreateBuffer(set->device, &cb_desc, &init,
                                   &set->lookup_buffer);
    set->last_hresult = hr;
    if (FAILED(hr) || set->lookup_buffer == NULL) {
        set->lookup_buffer = NULL;
        ID3D11SamplerState_Release(set->sampler);
        set->sampler = NULL;
        ID3D11ShaderResourceView_Release(set->srv);
        set->srv = NULL;
        ID3D11Texture2D_Release(set->atlas);
        set->atlas = NULL;
        return GEX_D3D11_TEXTURE_CREATE_FAILED;
    }

    set->initialized = 1;
    set->last_hresult = S_OK;
    return GEX_D3D11_TEXTURE_OK;
}

void gex_d3d11_texture_set_destroy(gex_d3d11_texture_set *set)
{
    if (set == NULL) {
        return;
    }
    if (set->lookup_buffer != NULL) {
        ID3D11Buffer_Release(set->lookup_buffer);
    }
    if (set->sampler != NULL) {
        ID3D11SamplerState_Release(set->sampler);
    }
    if (set->srv != NULL) {
        ID3D11ShaderResourceView_Release(set->srv);
    }
    if (set->atlas != NULL) {
        ID3D11Texture2D_Release(set->atlas);
    }
    memset(set, 0, sizeof(*set));
}

const char *gex_d3d11_texture_status_name(gex_d3d11_texture_status status)
{
    switch (status) {
    case GEX_D3D11_TEXTURE_OK:
        return "ok";
    case GEX_D3D11_TEXTURE_BAD_ARGUMENT:
        return "bad argument";
    case GEX_D3D11_TEXTURE_BAD_STATE:
        return "bad state";
    case GEX_D3D11_TEXTURE_TOO_MANY:
        return "too many textures";
    case GEX_D3D11_TEXTURE_CREATE_FAILED:
        return "resource creation failed";
    case GEX_D3D11_TEXTURE_OUT_OF_MEMORY:
        return "out of memory";
    }
    return "unknown";
}

#endif /* _WIN32 */
