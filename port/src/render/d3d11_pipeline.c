/*
 * d3d11_pipeline.c - Módulo 5A: pipeline mínimo de dibujo D3D11
 * (Windows-only).
 *
 * Contenido:
 *   - el HLSL del vertex shader y del pixel shader (color por vértice);
 *   - la compilación en ejecución con d3dcompiler_47.dll;
 *   - el ID3D11InputLayout que refleja los 28 B de `gex_vertex`;
 *   - los estados de rasterizado, blending y profundidad;
 *   - el constant buffer con el viewport y la resolución de referencia;
 *   - el dibujo con DrawIndexed.
 *
 * No dibuja nada por su cuenta: el llamador fija el destino y luego llama a
 * `gex_d3d11_pipeline_draw`. No hay texturas ni shaders de materiales NCC.
 *
 * Véase gex/render/d3d11_pipeline.h y docs/D3D11_FIRST_SCENE_PLAN.md §14.
 */

#include "gex/render/d3d11_pipeline.h"

#if defined(_WIN32)

#include <math.h>
#include <stdio.h>
#include <string.h>

#include <d3dcompiler.h>

/* ========================================================================
 * HLSL
 * ======================================================================== */

/*
 * Vertex shader.
 *
 * El vértice de la emisión lleva X/Y ya proyectadas en píxeles del espacio
 * lógico del original (512x384, centro en 256/192) y Z como profundidad
 * entera. Aquí NO se aplica ninguna matriz de cámara: solo se convierte de
 * píxeles lógicos a espacio de clip, que es lo que exige el rasterizador.
 *
 * El escalado es uniforme (min(vw/rw, vh/rh)) y centrado, de modo que la
 * imagen lógica no se deforma al cambiar el tamaño de la ventana.
 *
 * El color se lee como R8G8B8A8_UNORM (D3D11 no admite B8G8R8A8_UNORM en un
 * vertex buffer). El uint32 del vértice es AARRGGBB en little-endian, así que
 * en memoria los bytes son BB, GG, RR, AA: el shader recibe (B,G,R,A) en los
 * canales R,G,B,A y hay que reordenarlos.
 *
 * Se pasan además la coordenada de textura (TEXCOORD) y el índice de textura
 * (TEXINDEX). El índice es un dato POR POLÍGONO: viaja en los tres vértices
 * con el mismo valor y se declara `nointerpolation` para que el rasterizador
 * no lo interpole.
 */
static const char GEX_HLSL_VERTEX[] =
    "struct VSIn {\n"
    "    float3 pos   : POSITION;\n"
    "    float4 color : COLOR;\n"
    "    float2 uv    : TEXCOORD;\n"
    "    uint   texIndex : TEXINDEX;\n"
    "    int    layer  : LAYER;\n"
    "    uint   flags  : FLAGS;\n"
    "};\n"
    "\n"
    "struct VSOut {\n"
    "    float4 pos   : SV_POSITION;\n"
    "    float4 color : COLOR;\n"
    "    float2 uv    : TEXCOORD0;\n"
    "    nointerpolation uint texIndex : TEXCOORD1;\n"
    "};\n"
    "\n"
    "cbuffer GexView : register(b0) {\n"
    "    float4 viewport_size;  /* xy = viewport en pixeles, z = divisor de z */\n"
    "    float4 reference_size; /* xy = resolucion logica de referencia */\n"
    "};\n"
    "\n"
    "VSOut GexVS(VSIn input)\n"
    "{\n"
    "    VSOut output;\n"
    "    float2 half_ref = reference_size.xy * 0.5;\n"
    "    float2 p = input.pos.xy - half_ref;\n"
    "    float scale = min(viewport_size.x / reference_size.x,\n"
    "                      viewport_size.y / reference_size.y);\n"
    "    float2 ndc;\n"
    "    ndc.x = (p.x * scale * 2.0) / viewport_size.x;\n"
    "    ndc.y = (-p.y * scale * 2.0) / viewport_size.y;\n"
    "    output.pos = float4(ndc, input.pos.z / viewport_size.z, 1.0);\n"
    "    /* AARRGGBB leido como R8G8B8A8_UNORM llega como (B,G,R,A). */\n"
    "    output.color = float4(input.color.b, input.color.g,\n"
    "                           input.color.r, input.color.a);\n"
    "    output.uv = input.uv;\n"
    "    output.texIndex = input.texIndex;\n"
    "    return output;\n"
    "}\n";

/*
 * Pixel shader: muestrea la textura y la modula con el color por vértice.
 *
 * Las texturas del nivel están en un **atlas** (una textura 2D con todas
 * dentro). El índice de textura del `.VFX` (14 bits) se busca en la tabla
 * constante `GexTex`, que da el tamaño y la posición de la textura dentro
 * del atlas.
 *
 * La coordenada del vértice es `byte_uv / 256` (el pipeline la normaliza así).
 * El espacio de coordenadas del original es **256 unidades = lado mayor de la
 * textura** (S-5): `s = byte / 16` con `oow = 1/16` da `s = byte`, y Glide
 * interpreta 0..256 como la textura entera en su lado mayor. Por tanto la
 * posición del texel es `uv * max(w, h)` en AMBOS ejes (escala uniforme), no
 * `uv * w` y `uv * h` por separado: para una textura 64x32 el eje corto
 * recorre 0..127 bytes, que son 0..31 texeles. Con `uv * 256` (la lectura
 * anterior) cada textura se repetía `256/lado` veces: era el origen de los
 * mosaicos y de la pantalla del televisor en cuatro partes.
 *
 * Si no hay texturas (el camino del cubo de prueba) la tabla está vacía y el
 * shader devuelve el color por vértice tal cual, sin tocar nada.
 *
 * La modulación es la del original: el color final es
 * `textura * color por vertice` (modulate).
 *
 * EL ALFA NO VIENE DEL VÉRTICE (docs/D3D11_FIRST_SCENE_PLAN.md § 19.11). El
 * vértice que el juego entrega a Glide lleva el color en **tres** flotantes
 * (B, G, R) y ningún canal alfa: el byte alto del color del comando
 * (`0x34`/`0xB4`/`0x36`/`0xB6`) es donde el emisor empaqueta el opcode
 * (`0xB4 | ((color0 >> 24) & 3)`), y ese byte nunca llega a la GPU. En el
 * driver real (nGlide 2.10) el alfa de la unidad de mezcla es una
 * **constante 1.0**: con el *alpha test* activo (el juego lo habilita para las
 * texturas opacas) un alfa 0 descartaría toda la geometría, y con el estado
 * `SRC_ALPHA`/`INV_SRC_ALPHA` un alfa distinto de 1 haría translúcida toda la
 * escena. Por tanto el alfa de salida es **el alfa del texel** (y 1.0 cuando
 * la textura no tiene canal alfa o cuando no hay textura), nunca el byte alto
 * del color por vértice.
 */
static const char GEX_HLSL_PIXEL[] =
    "struct VSOut {\n"
    "    float4 pos   : SV_POSITION;\n"
    "    float4 color : COLOR;\n"
    "    float2 uv    : TEXCOORD0;\n"
    "    nointerpolation uint texIndex : TEXCOORD1;\n"
    "};\n"
    "\n"
    "cbuffer GexTex : register(b1) {\n"
    "    float4 gex_tex_header;\n"
    "    float4 gex_tex_entries[512];\n"
    "};\n"
    "\n"
    "Texture2D gex_tex_atlas : register(t0);\n"
    "SamplerState gex_tex_sampler : register(s0);\n"
    "\n"
    "float4 GexPS(VSOut input) : SV_TARGET\n"
    "{\n"
    "    float4 color = input.color;\n"
    "    color.a = 1.0;\n"
    "    int count = (int)gex_tex_header.x;\n"
    "    float atlas_w = gex_tex_header.y;\n"
    "    float atlas_h = gex_tex_header.z;\n"
    "    int mode = (int)gex_tex_header.w;\n"
    "    for (int i = 0; i < count; i++) {\n"
    "        float4 a = gex_tex_entries[i * 2];\n"
    "        float4 b = gex_tex_entries[i * 2 + 1];\n"
    "        if ((uint)a.x == input.texIndex) {\n"
    "            float2 size = float2(a.y, a.z);\n"
    "            float2 origin = float2(a.w, b.x);\n"
    "            float2 texels;\n"
    "            if (mode == 1) {\n"
    "                /* Modo texel (el del original, S-5): el byte de UV es\n"
    "                   1/256 del lado MAYOR de la textura, con la misma\n"
    "                   escala en los dos ejes. */\n"
    "                float scale = max(size.x, size.y);\n"
    "                texels = input.uv * scale;\n"
    "                texels = texels - floor(texels / size) * size;\n"
    "            } else {\n"
    "                /* Modo normalizado: el byte de UV es una fraccion de\n"
    "                   cada eje por separado (256 = la textura entera). */\n"
    "                texels = input.uv * size;\n"
    "            }\n"
    "            /* D3D11 muestrea en el centro del texel: se suma media\n"
    "             * texel para que caiga en el mismo texel que el\n"
    "             * original. */\n"
    "            float2 atlas_uv = (origin + texels + 0.5) /\n"
    "                             float2(atlas_w, atlas_h);\n"
    "            float4 t = gex_tex_atlas.SampleLevel(gex_tex_sampler,\n"
    "                                                atlas_uv, 0);\n"
    "            color = float4(color.rgb * t.rgb, t.a);\n"
    "            /* Modo de mezcla por poligono (0x46558A en el original):\n"
    "             * con canal alfa (ARGB_4444) el triangulo se MEZCLA con el\n"
    "             * destino usando el alfa de la textura; sin canal alfa el\n"
    "             * triangulo es OPACO y substituye el destino. Con el\n"
    "             * estado de mezcla global SRC_ALPHA/INV_SRC_ALPHA la\n"
    "             * opacidad se consigue con un alfa de salida 1\n"
    "             * (src*1 + dst*0 = src). El alfa de salida NO incluye el\n"
    "             * byte alto del color por vertice (plan 19.11). */\n"
    "            if (b.y < 0.5) {\n"
    "                color.a = 1.0;\n"
    "            }\n"
    "            break;\n"
    "        }\n"
    "    }\n"
    "    return color;\n"
    "}\n";

/* ========================================================================
 * Layout de entrada
 * ======================================================================== */

/*
 * Descripción de entrada. Ojo con la API: `D3D11_INPUT_ELEMENT_DESC` **no**
 * tiene campo `Offset`; el byte offset dentro del vertex buffer va en
 * `AlignedByteOffset` (el `D3D11_APPEND_ALIGNED_ELEMENT` permite encadenar
 * elementos). `InputSlot` es el slot del vertex buffer.
 */
const D3D11_INPUT_ELEMENT_DESC
    gex_d3d11_input_layout[GEX_D3D11_INPUT_ELEMENT_COUNT] = {
        { "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0,
          GEX_VERTEX_OFFSET_POSITION, D3D11_INPUT_PER_VERTEX_DATA, 0 },
        { "COLOR",    0, DXGI_FORMAT_R8G8B8A8_UNORM,   0,
          GEX_VERTEX_OFFSET_COLOR,    D3D11_INPUT_PER_VERTEX_DATA, 0 },
        { "TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT,     0,
          GEX_VERTEX_OFFSET_UV,       D3D11_INPUT_PER_VERTEX_DATA, 0 },
        { "TEXINDEX", 0, DXGI_FORMAT_R16_UINT,        0,
          GEX_VERTEX_OFFSET_TEXTURE,  D3D11_INPUT_PER_VERTEX_DATA, 0 },
        { "LAYER",    0, DXGI_FORMAT_R8_SINT,         0,
          GEX_VERTEX_OFFSET_LAYER,    D3D11_INPUT_PER_VERTEX_DATA, 0 },
        { "FLAGS",    0, DXGI_FORMAT_R8_UINT,         0,
          GEX_VERTEX_OFFSET_FLAGS,    D3D11_INPUT_PER_VERTEX_DATA, 0 }
    };

/* ========================================================================
 * Constant buffer
 * ======================================================================== */

typedef struct {
    float viewport_size[4];  /* x,y = viewport en píxeles; z = divisor de z */
    float reference_size[4]; /* x,y = resolución lógica de referencia */
} gex_d3d11_constants;

/* ========================================================================
 * Compilación HLSL en ejecución
 * ======================================================================== */

typedef HRESULT (WINAPI *gex_d3dcompile_fn)(
    const void *data, SIZE_T data_size, const char *filename,
    const D3D_SHADER_MACRO *defines, ID3DInclude *include,
    const char *entrypoint, const char *target, UINT sflags, UINT eflags,
    ID3DBlob **shader, ID3DBlob **error_messages);

static void copy_error(char *dst, size_t dst_size, ID3DBlob *errors)
{
    const char *text;
    size_t len;

    dst[0] = '\0';
    if (errors == NULL) {
        return;
    }
    text = (const char *)ID3D10Blob_GetBufferPointer(errors);
    if (text == NULL) {
        return;
    }
    len = ID3D10Blob_GetBufferSize(errors);
    if (len > dst_size - 1u) {
        len = dst_size - 1u;
    }
    memcpy(dst, text, len);
    dst[len] = '\0';
    /* El blob suele terminar en salto de línea; se recorta. */
    while (len > 0 && (dst[len - 1] == '\n' || dst[len - 1] == '\r')) {
        dst[--len] = '\0';
    }
}

/* ========================================================================
 * Inyección de fallos (solo pruebas)
 * ======================================================================== */

/*
 * Mismo patrón que `GEX_BSP_TEST_FAULT_INJECT` y
 * `GEX_GEOM_TEST_FAULT_INJECT`: una variable externa permite hacer fallar una
 * etapa concreta de la inicialización para comprobar que no quedan recursos
 * vivos a medias. El valor N (1..GEX_PIPELINE_FAULT_STAGES) hace fallar esa
 * etapa; 0 desactiva la inyección.
 */
#define GEX_PIPELINE_FAULT_STAGES 10u

#ifdef GEX_PIPELINE_TEST_FAULT_INJECT
extern int gex_d3d11_pipeline_test_fail_stage;
int gex_d3d11_pipeline_test_fail_stage = 0;
static int fault_stage_count;

static int fault_at(unsigned int stage)
{
    fault_stage_count++;
    return (gex_d3d11_pipeline_test_fail_stage > 0 &&
            (unsigned int)gex_d3d11_pipeline_test_fail_stage == stage);
}
#else
static int fault_at(unsigned int stage)
{
    (void)stage;
    return 0;
}
#endif

static void release_blob(ID3DBlob **blob)
{
    if (blob != NULL && *blob != NULL) {
        ID3D10Blob_Release(*blob);
        *blob = NULL;
    }
}

/*
 * Descriptor D3D11 de cada modo de mezcla por comando (S-7). Traduce los
 * factores nGlide 2.10 del original (§19.8 del plan): 0 = ZERO, 1 =
 * SRC_ALPHA, 4 = ONE, 5 = INV_SRC_ALPHA, 6 = INV_DEST_COLOR. El original no
 * tiene alfa de destino propio: el canal alfa se escribe con ONE/ZERO cuando
 * la cuádrupla lo deja en 4/0, y con ZERO/ZERO cuando lo apaga (0/0).
 */
static void blend_desc_for_mode(gex_blend_mode mode, D3D11_BLEND_DESC *desc)
{
    D3D11_RENDER_TARGET_BLEND_DESC *rt;

    memset(desc, 0, sizeof(*desc));
    desc->AlphaToCoverageEnable = FALSE;
    desc->IndependentBlendEnable = FALSE;
    rt = &desc->RenderTarget[0];
    rt->BlendEnable = TRUE;
    rt->BlendOp = D3D11_BLEND_OP_ADD;
    rt->BlendOpAlpha = D3D11_BLEND_OP_ADD;
    rt->RenderTargetWriteMask = D3D11_COLOR_WRITE_ENABLE_ALL;

    switch (mode) {
    case GEX_BLEND_ADDITIVE:             /* (1,4,4,0) */
        rt->SrcBlend = D3D11_BLEND_SRC_ALPHA;
        rt->DestBlend = D3D11_BLEND_ONE;
        rt->SrcBlendAlpha = D3D11_BLEND_ONE;
        rt->DestBlendAlpha = D3D11_BLEND_ZERO;
        break;
    case GEX_BLEND_OPAQUE:               /* (4,0,0,0) */
        rt->SrcBlend = D3D11_BLEND_ONE;
        rt->DestBlend = D3D11_BLEND_ZERO;
        rt->SrcBlendAlpha = D3D11_BLEND_ONE;
        rt->DestBlendAlpha = D3D11_BLEND_ZERO;
        break;
    case GEX_BLEND_ERASE:                /* (0,6,0,0) */
        rt->SrcBlend = D3D11_BLEND_ZERO;
        rt->DestBlend = D3D11_BLEND_INV_DEST_COLOR;
        rt->SrcBlendAlpha = D3D11_BLEND_ZERO;
        rt->DestBlendAlpha = D3D11_BLEND_ZERO;
        break;
    case GEX_BLEND_ADDITIVE_ALPHA_OFF:   /* (1,4,0,0) */
        rt->SrcBlend = D3D11_BLEND_SRC_ALPHA;
        rt->DestBlend = D3D11_BLEND_ONE;
        rt->SrcBlendAlpha = D3D11_BLEND_ZERO;
        rt->DestBlendAlpha = D3D11_BLEND_ZERO;
        break;
    case GEX_BLEND_ALPHA:                /* (1,5,4,0) */
    default:
        rt->SrcBlend = D3D11_BLEND_SRC_ALPHA;
        rt->DestBlend = D3D11_BLEND_INV_SRC_ALPHA;
        rt->SrcBlendAlpha = D3D11_BLEND_ONE;
        rt->DestBlendAlpha = D3D11_BLEND_ZERO;
        break;
    }
}

/* ========================================================================
 * Ciclo de vida
 * ======================================================================== */

static void release_all(gex_d3d11_pipeline *pipeline)
{
    if (pipeline->tex_constants != NULL) {
        ID3D11Buffer_Release(pipeline->tex_constants);
    }
    if (pipeline->constant_buffer != NULL) {
        ID3D11Buffer_Release(pipeline->constant_buffer);
    }
    if (pipeline->depth_stencil != NULL) {
        ID3D11DepthStencilState_Release(pipeline->depth_stencil);
    }
    {
        /* `blend` es el alias de `blends[ALPHA]`: se libera una sola vez. */
        ID3D11BlendState *alpha = pipeline->blend;
        int mode;
        for (mode = 0; mode < GEX_BLEND_MODE_COUNT; mode++) {
            if (pipeline->blends[mode] != NULL &&
                pipeline->blends[mode] != alpha) {
                ID3D11BlendState_Release(pipeline->blends[mode]);
            }
            pipeline->blends[mode] = NULL;
        }
        if (alpha != NULL) {
            ID3D11BlendState_Release(alpha);
        }
        pipeline->blend = NULL;
    }
    if (pipeline->rasterizer != NULL) {
        ID3D11RasterizerState_Release(pipeline->rasterizer);
    }
    if (pipeline->input_layout != NULL) {
        ID3D11InputLayout_Release(pipeline->input_layout);
    }
    if (pipeline->pixel_shader != NULL) {
        ID3D11PixelShader_Release(pipeline->pixel_shader);
    }
    if (pipeline->vertex_shader != NULL) {
        ID3D11VertexShader_Release(pipeline->vertex_shader);
    }
    if (pipeline->compiler_module != NULL) {
        FreeLibrary(pipeline->compiler_module);
    }
    memset(pipeline, 0, sizeof(*pipeline));
}

/*
 * Fallo de inicialización: limpia todos los recursos **sin perder el
 * diagnóstico**. `release_all` deja la estructura a cero, así que el mensaje
 * y el HRESULT se copian a variables locales y se restauran después: sin
 * esto, el texto del error de compilación del HLSL se perdería justo cuando
 * hace falta.
 */
static gex_d3d11_pipeline_status fail_init(gex_d3d11_pipeline *pipeline,
                                           gex_d3d11_pipeline_status status,
                                           HRESULT hr,
                                           const char *message)
{
    char text[sizeof(pipeline->last_error)];
    HRESULT code = hr;

    memset(text, 0, sizeof(text));
    if (message != NULL) {
        size_t len = strlen(message);
        if (len >= sizeof(text)) {
            len = sizeof(text) - 1u;
        }
        memcpy(text, message, len);
    }
    if (text[0] == '\0') {
        snprintf(text, sizeof(text), "fallo de inicializacion del pipeline");
    }
    release_all(pipeline);
    pipeline->last_hresult = code;
    memcpy(pipeline->last_error, text, sizeof(text));
    return status;
}

gex_d3d11_pipeline_status gex_d3d11_pipeline_init(gex_d3d11_pipeline *pipeline,
                                                   const gex_d3d11_device *device)
{
    D3D11_RASTERIZER_DESC raster_desc;
    D3D11_BLEND_DESC blend_desc;
    D3D11_DEPTH_STENCIL_DESC depth_desc;
    D3D11_BUFFER_DESC cb_desc;
    gex_d3dcompile_fn compile_fn;
    FARPROC proc;
    ID3DBlob *vs_code = NULL;
    ID3DBlob *ps_code = NULL;
    ID3DBlob *errors = NULL;
    HRESULT hr;
    DWORD load_error;
    char message[sizeof(pipeline->last_error)];

#ifdef GEX_PIPELINE_TEST_FAULT_INJECT
    fault_stage_count = 0;
#endif

    if (pipeline == NULL || device == NULL) {
        return GEX_D3D11_PIPELINE_BAD_ARGUMENT;
    }
    if (pipeline->initialized) {
        return GEX_D3D11_PIPELINE_BAD_STATE;
    }
    memset(pipeline, 0, sizeof(*pipeline));
    pipeline->last_hresult = S_OK;
    if (!device->initialized || device->device == NULL ||
        device->context == NULL) {
        pipeline->last_hresult = E_FAIL;
        snprintf(pipeline->last_error, sizeof(pipeline->last_error),
                 "el dispositivo no esta inicializado");
        return GEX_D3D11_PIPELINE_BAD_DEVICE;
    }

    pipeline->device = device->device;
    pipeline->context = device->context;

    /* d3dcompiler_47.dll: el compilador HLSL que forma parte de Windows.
     * Se carga bajo demanda para que la aplicación no dependa de él en el
     * arranque y pueda informar del problema con claridad. */
    load_error = 0;
    if (!fault_at(1u)) {
        pipeline->compiler_module = LoadLibraryA(D3DCOMPILER_DLL_A);
        if (pipeline->compiler_module == NULL) {
            load_error = GetLastError();
        }
    }
    if (pipeline->compiler_module == NULL) {
        HRESULT code = (load_error != 0)
                           ? HRESULT_FROM_WIN32(load_error)
                           : HRESULT_FROM_WIN32(ERROR_MOD_NOT_FOUND);
        snprintf(message, sizeof(message), "no se pudo cargar %s",
                 D3DCOMPILER_DLL_A);
        return fail_init(pipeline, GEX_D3D11_PIPELINE_NO_COMPILER, code,
                         message);
    }
    proc = GetProcAddress(pipeline->compiler_module, "D3DCompile");
    if (proc == NULL) {
        snprintf(message, sizeof(message), "%s no exporta D3DCompile",
                 D3DCOMPILER_DLL_A);
        return fail_init(pipeline, GEX_D3D11_PIPELINE_NO_COMPILER,
                         HRESULT_FROM_WIN32(ERROR_PROC_NOT_FOUND), message);
    }
    compile_fn = (gex_d3dcompile_fn)(void *)proc;

    hr = compile_fn(GEX_HLSL_VERTEX, sizeof(GEX_HLSL_VERTEX) - 1u,
                    "gex_vertex.hlsl", NULL, NULL, "GexVS", "vs_4_0",
                    D3DCOMPILE_ENABLE_STRICTNESS, 0, &vs_code, &errors);
    if (fault_at(2u)) {
        hr = E_FAIL;
        release_blob(&vs_code);
    }
    if (FAILED(hr) || vs_code == NULL) {
        memset(message, 0, sizeof(message));
        copy_error(message, sizeof(message), errors);
        release_blob(&errors);
        release_blob(&vs_code);
        return fail_init(pipeline, GEX_D3D11_PIPELINE_SHADER_FAILED, hr,
                         message);
    }
    release_blob(&errors);

    hr = compile_fn(GEX_HLSL_PIXEL, sizeof(GEX_HLSL_PIXEL) - 1u,
                    "gex_pixel.hlsl", NULL, NULL, "GexPS", "ps_4_0",
                    D3DCOMPILE_ENABLE_STRICTNESS, 0, &ps_code, &errors);
    if (fault_at(3u)) {
        hr = E_FAIL;
        release_blob(&ps_code);
    }
    if (FAILED(hr) || ps_code == NULL) {
        memset(message, 0, sizeof(message));
        copy_error(message, sizeof(message), errors);
        release_blob(&errors);
        release_blob(&ps_code);
        release_blob(&vs_code);
        return fail_init(pipeline, GEX_D3D11_PIPELINE_SHADER_FAILED, hr,
                         message);
    }
    release_blob(&errors);

    hr = ID3D11Device_CreateVertexShader(
        pipeline->device, ID3D10Blob_GetBufferPointer(vs_code),
        (UINT)ID3D10Blob_GetBufferSize(vs_code), NULL,
        &pipeline->vertex_shader);
    if (fault_at(4u)) {
        hr = E_FAIL;
        if (pipeline->vertex_shader != NULL) {
            ID3D11VertexShader_Release(pipeline->vertex_shader);
            pipeline->vertex_shader = NULL;
        }
    }
    if (FAILED(hr) || pipeline->vertex_shader == NULL) {
        release_blob(&ps_code);
        release_blob(&vs_code);
        snprintf(message, sizeof(message), "no se pudo crear el vertex shader");
        return fail_init(pipeline, GEX_D3D11_PIPELINE_CREATE_FAILED, hr,
                         message);
    }

    hr = ID3D11Device_CreatePixelShader(
        pipeline->device, ID3D10Blob_GetBufferPointer(ps_code),
        (UINT)ID3D10Blob_GetBufferSize(ps_code), NULL,
        &pipeline->pixel_shader);
    if (fault_at(5u)) {
        hr = E_FAIL;
        if (pipeline->pixel_shader != NULL) {
            ID3D11PixelShader_Release(pipeline->pixel_shader);
            pipeline->pixel_shader = NULL;
        }
    }
    if (FAILED(hr) || pipeline->pixel_shader == NULL) {
        release_blob(&ps_code);
        release_blob(&vs_code);
        snprintf(message, sizeof(message), "no se pudo crear el pixel shader");
        return fail_init(pipeline, GEX_D3D11_PIPELINE_CREATE_FAILED, hr,
                         message);
    }

    /* El layout se valida contra la firma del vertex shader: por eso el
     * blob del VS sigue vivo hasta aquí. */
    hr = ID3D11Device_CreateInputLayout(
        pipeline->device, gex_d3d11_input_layout,
        (UINT)GEX_D3D11_INPUT_ELEMENT_COUNT,
        ID3D10Blob_GetBufferPointer(vs_code),
        (UINT)ID3D10Blob_GetBufferSize(vs_code),
        &pipeline->input_layout);
    if (fault_at(6u)) {
        hr = E_FAIL;
        if (pipeline->input_layout != NULL) {
            ID3D11InputLayout_Release(pipeline->input_layout);
            pipeline->input_layout = NULL;
        }
    }
    if (FAILED(hr) || pipeline->input_layout == NULL) {
        release_blob(&ps_code);
        release_blob(&vs_code);
        snprintf(message, sizeof(message), "no se pudo crear el input layout");
        return fail_init(pipeline, GEX_D3D11_PIPELINE_CREATE_FAILED, hr,
                         message);
    }
    release_blob(&ps_code);
    release_blob(&vs_code);

    /* Rasterizador: relleno sólido, sin culling (la emisión ya descartó los
     * triángulos con el determinante 2D del original) y **con scissor**: el
     * original no tiene framebuffer fuera de 512x384, así que la geometría
     * proyectada fuera de la zona lógica no puede dibujarse (S-6). El
     * rectángulo lo fija `gex_d3d11_pipeline_set_target`. */
    memset(&raster_desc, 0, sizeof(raster_desc));
    raster_desc.FillMode = D3D11_FILL_SOLID;
    raster_desc.CullMode = D3D11_CULL_NONE;
    raster_desc.DepthClipEnable = TRUE;
    raster_desc.ScissorEnable = TRUE;
    raster_desc.AntialiasedLineEnable = FALSE;
    raster_desc.MultisampleEnable = FALSE;
    raster_desc.DepthBias = 0;
    raster_desc.DepthBiasClamp = 0.0f;
    raster_desc.SlopeScaledDepthBias = 0.0f;

    hr = ID3D11Device_CreateRasterizerState(pipeline->device, &raster_desc,
                                            &pipeline->rasterizer);
    if (fault_at(7u)) {
        hr = E_FAIL;
        if (pipeline->rasterizer != NULL) {
            ID3D11RasterizerState_Release(pipeline->rasterizer);
            pipeline->rasterizer = NULL;
        }
    }
    if (FAILED(hr) || pipeline->rasterizer == NULL) {
        snprintf(message, sizeof(message), "no se pudo crear el rasterizador");
        return fail_init(pipeline, GEX_D3D11_PIPELINE_CREATE_FAILED, hr,
                         message);
    }

    /* Blending por comando (S-7): un estado por modo, igual que el original
     * cambia grAlphaBlendFunction antes de cada comando. El estado ALPHA
     * (SRC_ALPHA / INV_SRC_ALPHA, alfa ONE/ZERO) es el que el port usaba para
     * todo el dibujo y sigue siendo el de `pipeline->blend`, el menú y las
     * formas sintéticas. Los cinco se crean en el mismo stage de fallo: un
     * fallo inyectado en 8 no deja ninguno vivo. */
    {
        int inject = fault_at(8u);
        int mode;
        for (mode = 0; mode < GEX_BLEND_MODE_COUNT; mode++) {
            blend_desc_for_mode((gex_blend_mode)mode, &blend_desc);
            hr = ID3D11Device_CreateBlendState(
                pipeline->device, &blend_desc, &pipeline->blends[mode]);
            if (inject) {
                hr = E_FAIL;
                if (pipeline->blends[mode] != NULL) {
                    ID3D11BlendState_Release(pipeline->blends[mode]);
                    pipeline->blends[mode] = NULL;
                }
            }
            if (FAILED(hr) || pipeline->blends[mode] == NULL) {
                snprintf(message, sizeof(message),
                         "no se pudo crear el estado de blend");
                return fail_init(pipeline, GEX_D3D11_PIPELINE_CREATE_FAILED,
                                 hr, message);
            }
        }
        pipeline->blend = pipeline->blends[GEX_BLEND_ALPHA];
    }

    /* Sin depth buffer: el orden de dibujo es el orden de emisión
     * (algoritmo del pintor), igual que en el original. */
    memset(&depth_desc, 0, sizeof(depth_desc));
    depth_desc.DepthEnable = FALSE;
    depth_desc.DepthWriteMask = D3D11_DEPTH_WRITE_MASK_ZERO;
    depth_desc.DepthFunc = D3D11_COMPARISON_ALWAYS;
    depth_desc.StencilEnable = FALSE;

    hr = ID3D11Device_CreateDepthStencilState(pipeline->device, &depth_desc,
                                              &pipeline->depth_stencil);
    if (fault_at(9u)) {
        hr = E_FAIL;
        if (pipeline->depth_stencil != NULL) {
            ID3D11DepthStencilState_Release(pipeline->depth_stencil);
            pipeline->depth_stencil = NULL;
        }
    }
    if (FAILED(hr) || pipeline->depth_stencil == NULL) {
        snprintf(message, sizeof(message),
                 "no se pudo crear el estado de profundidad");
        return fail_init(pipeline, GEX_D3D11_PIPELINE_CREATE_FAILED, hr,
                         message);
    }

    /* Constant buffer con el viewport y la resolución de referencia. */
    memset(&cb_desc, 0, sizeof(cb_desc));
    cb_desc.ByteWidth = sizeof(gex_d3d11_constants);
    cb_desc.Usage = D3D11_USAGE_DYNAMIC;
    cb_desc.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
    cb_desc.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;
    cb_desc.MiscFlags = 0;
    cb_desc.StructureByteStride = 0;

    hr = ID3D11Device_CreateBuffer(pipeline->device, &cb_desc, NULL,
                                   &pipeline->constant_buffer);
    if (fault_at(10u)) {
        hr = E_FAIL;
        if (pipeline->constant_buffer != NULL) {
            ID3D11Buffer_Release(pipeline->constant_buffer);
            pipeline->constant_buffer = NULL;
        }
    }
    if (FAILED(hr) || pipeline->constant_buffer == NULL) {
        snprintf(message, sizeof(message), "no se pudo crear el constant buffer");
        return fail_init(pipeline, GEX_D3D11_PIPELINE_CREATE_FAILED, hr,
                         message);
    }

    /* Constant buffer de la tabla de texturas (b1). Se crea siempre, aunque
     * no haya texturas: el pixel shader lee su cabecera y, con el número de
     * entradas a cero, devuelve el color por vértice sin muestrear nada. */
    memset(&cb_desc, 0, sizeof(cb_desc));
    cb_desc.ByteWidth = 16u; /* solo la cabecera float4 */
    cb_desc.Usage = D3D11_USAGE_IMMUTABLE;
    cb_desc.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
    cb_desc.CPUAccessFlags = 0;
    cb_desc.MiscFlags = 0;
    cb_desc.StructureByteStride = 0;

    {
        D3D11_SUBRESOURCE_DATA init;
        float header[4] = { 0.0f, 0.0f, 0.0f, 0.0f };
        memset(&init, 0, sizeof(init));
        init.pSysMem = header;
        init.SysMemPitch = 0;
        init.SysMemSlicePitch = 0;
        hr = ID3D11Device_CreateBuffer(pipeline->device, &cb_desc, &init,
                                       &pipeline->tex_constants);
    }
    if (FAILED(hr) || pipeline->tex_constants == NULL) {
        pipeline->tex_constants = NULL;
        snprintf(message, sizeof(message),
                 "no se pudo crear el constant buffer de texturas");
        return fail_init(pipeline, GEX_D3D11_PIPELINE_CREATE_FAILED, hr,
                         message);
    }

    pipeline->initialized = 1;
    pipeline->last_hresult = S_OK;
    return GEX_D3D11_PIPELINE_OK;
}

void gex_d3d11_pipeline_shutdown(gex_d3d11_pipeline *pipeline)
{
    if (pipeline == NULL) {
        return;
    }
    release_all(pipeline);
}

/* ========================================================================
 * Destino de dibujo
 * ======================================================================== */

int gex_d3d11_pipeline_logical_rect(UINT width, UINT height,
                                    UINT ref_width, UINT ref_height,
                                    gex_d3d11_logical_rect *out)
{
    float scale, fw, fh, fx, fy;
    int32_t x0, y0, x1, y1;

    if (out == NULL) {
        return 0;
    }
    out->x = 0;
    out->y = 0;
    out->width = 0u;
    out->height = 0u;
    if (width == 0u || height == 0u || ref_width == 0u || ref_height == 0u) {
        return 0;
    }

    /* Misma aritmética que el vertex shader: escala uniforme y centrado. */
    scale = (float)width / (float)ref_width;
    if ((float)height / (float)ref_height < scale) {
        scale = (float)height / (float)ref_height;
    }
    fw = (float)ref_width * scale;
    fh = (float)ref_height * scale;
    fx = ((float)width - fw) * 0.5f;
    fy = ((float)height - fh) * 0.5f;

    /* Hacia fuera: un píxel del borde lógico cae dentro del rectángulo. */
    x0 = (int32_t)floorf(fx);
    y0 = (int32_t)floorf(fy);
    x1 = (int32_t)ceilf(fx + fw);
    y1 = (int32_t)ceilf(fy + fh);

    if (x0 < 0) x0 = 0;
    if (y0 < 0) y0 = 0;
    if (x1 > (int32_t)width) x1 = (int32_t)width;
    if (y1 > (int32_t)height) y1 = (int32_t)height;
    if (x1 < x0) x1 = x0;
    if (y1 < y0) y1 = y0;

    out->x = x0;
    out->y = y0;
    out->width = (uint32_t)(x1 - x0);
    out->height = (uint32_t)(y1 - y0);
    return 1;
}

gex_d3d11_pipeline_status gex_d3d11_pipeline_set_target(
    gex_d3d11_pipeline *pipeline,
    ID3D11RenderTargetView *rtv,
    UINT width, UINT height,
    UINT ref_width, UINT ref_height)
{
    D3D11_VIEWPORT viewport;
    D3D11_RECT scissor;
    gex_d3d11_logical_rect logical;
    D3D11_MAPPED_SUBRESOURCE mapped;
    gex_d3d11_constants constants;
    HRESULT hr;

    if (pipeline == NULL || rtv == NULL) {
        return GEX_D3D11_PIPELINE_BAD_ARGUMENT;
    }
    if (!pipeline->initialized) {
        return GEX_D3D11_PIPELINE_BAD_STATE;
    }
    if (width == 0u || height == 0u || ref_width == 0u || ref_height == 0u) {
        return GEX_D3D11_PIPELINE_BAD_ARGUMENT;
    }

    viewport.TopLeftX = 0.0f;
    viewport.TopLeftY = 0.0f;
    viewport.Width = (float)width;
    viewport.Height = (float)height;
    viewport.MinDepth = 0.0f;
    viewport.MaxDepth = 1.0f;

    constants.viewport_size[0] = (float)width;
    constants.viewport_size[1] = (float)height;
    constants.viewport_size[2] = GEX_D3D11_DEPTH_DIVISOR;
    constants.viewport_size[3] = 1.0f;
    constants.reference_size[0] = (float)ref_width;
    constants.reference_size[1] = (float)ref_height;
    constants.reference_size[2] = 0.0f;
    constants.reference_size[3] = 0.0f;

    hr = ID3D11DeviceContext_Map(pipeline->context,
                                  (ID3D11Resource *)pipeline->constant_buffer,
                                  0, D3D11_MAP_WRITE_DISCARD, 0, &mapped);
    pipeline->last_hresult = hr;
    if (FAILED(hr)) {
        return GEX_D3D11_PIPELINE_CREATE_FAILED;
    }
    memcpy(mapped.pData, &constants, sizeof(constants));
    ID3D11DeviceContext_Unmap(pipeline->context,
                              (ID3D11Resource *)pipeline->constant_buffer, 0);

    ID3D11DeviceContext_OMSetRenderTargets(pipeline->context, 1, &rtv, NULL);
    ID3D11DeviceContext_RSSetViewports(pipeline->context, 1, &viewport);

    /* Recorte duro al borde del framebuffer original (512x384). El
     * rasterizador tiene ScissorEnable = TRUE, así que este rectángulo es
     * obligatorio: sin él no se dibujaría nada. */
    if (gex_d3d11_pipeline_logical_rect(width, height, ref_width, ref_height,
                                        &logical)) {
        scissor.left = (LONG)logical.x;
        scissor.top = (LONG)logical.y;
        scissor.right = (LONG)(logical.x + (int32_t)logical.width);
        scissor.bottom = (LONG)(logical.y + (int32_t)logical.height);
        ID3D11DeviceContext_RSSetScissorRects(pipeline->context, 1, &scissor);
    }

    ID3D11DeviceContext_VSSetConstantBuffers(pipeline->context, 0, 1,
                                             &pipeline->constant_buffer);
    pipeline->last_hresult = S_OK;
    return GEX_D3D11_PIPELINE_OK;
}

/* ========================================================================
 * Texturas (Módulo 4C)
 * ======================================================================== */

gex_d3d11_pipeline_status gex_d3d11_pipeline_set_textures(
    gex_d3d11_pipeline *pipeline, const gex_d3d11_texture_set *set)
{
    if (pipeline == NULL) {
        return GEX_D3D11_PIPELINE_BAD_ARGUMENT;
    }
    if (!pipeline->initialized) {
        return GEX_D3D11_PIPELINE_BAD_STATE;
    }
    if (set != NULL && !set->initialized) {
        return GEX_D3D11_PIPELINE_BAD_ARGUMENT;
    }
    pipeline->textures = set;
    pipeline->last_hresult = S_OK;
    return GEX_D3D11_PIPELINE_OK;
}

/* ========================================================================
 * Dibujo
 * ======================================================================== */

gex_d3d11_pipeline_status gex_d3d11_pipeline_draw(gex_d3d11_pipeline *pipeline,
                                                   const gex_d3d11_mesh *mesh)
{
    UINT stride = GEX_VERTEX_STRIDE;
    UINT offset = 0;

    if (pipeline == NULL || mesh == NULL) {
        return GEX_D3D11_PIPELINE_BAD_ARGUMENT;
    }
    if (!pipeline->initialized) {
        return GEX_D3D11_PIPELINE_BAD_STATE;
    }
    if (!mesh->initialized || mesh->vertex_buffer == NULL ||
        mesh->index_buffer == NULL) {
        pipeline->last_hresult = E_INVALIDARG;
        return GEX_D3D11_PIPELINE_BAD_ARGUMENT;
    }
    if (mesh->index_count == 0u || mesh->vertex_count == 0u) {
        pipeline->last_hresult = E_INVALIDARG;
        return GEX_D3D11_PIPELINE_NO_GEOMETRY;
    }

    /* Estados */
    ID3D11DeviceContext_IASetInputLayout(pipeline->context,
                                          pipeline->input_layout);
    ID3D11DeviceContext_IASetPrimitiveTopology(
        pipeline->context, D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
    ID3D11DeviceContext_IASetVertexBuffers(pipeline->context, 0, 1,
                                           &mesh->vertex_buffer, &stride,
                                           &offset);
    ID3D11DeviceContext_IASetIndexBuffer(pipeline->context, mesh->index_buffer,
                                         mesh->index_format, 0);

    ID3D11DeviceContext_VSSetShader(pipeline->context, pipeline->vertex_shader,
                                    NULL, 0);
    ID3D11DeviceContext_PSSetShader(pipeline->context, pipeline->pixel_shader,
                                    NULL, 0);
    ID3D11DeviceContext_RSSetState(pipeline->context, pipeline->rasterizer);
    ID3D11DeviceContext_OMSetDepthStencilState(pipeline->context,
                                               pipeline->depth_stencil, 0);

    /* Texturas (Módulo 4C): el shader resource view, el sampler y la tabla
     * de búsqueda (constant buffer b1). Sin conjunto de texturas se enlaza la
     * tabla vacía del pipeline y nada más: el shader no muestrea nada. */
    if (pipeline->textures != NULL && pipeline->textures->initialized) {
        ID3D11DeviceContext_PSSetShaderResources(
            pipeline->context, 0, 1, &pipeline->textures->srv);
        ID3D11DeviceContext_PSSetSamplers(pipeline->context, 0, 1,
                                          &pipeline->textures->sampler);
        ID3D11DeviceContext_PSSetConstantBuffers(
            pipeline->context, 1, 1, &pipeline->textures->lookup_buffer);
    } else {
        /* Sin texturas: se desenlaza lo que hubiera (un NULL en el slot).
         * Ojo: la API pide un ARRAY de punteros, no un NULL suelto. */
        ID3D11ShaderResourceView *null_srv[1];
        ID3D11SamplerState *null_sampler[1];
        null_srv[0] = NULL;
        null_sampler[0] = NULL;
        ID3D11DeviceContext_PSSetShaderResources(pipeline->context, 0, 1,
                                                 null_srv);
        ID3D11DeviceContext_PSSetSamplers(pipeline->context, 0, 1,
                                          null_sampler);
        ID3D11DeviceContext_PSSetConstantBuffers(pipeline->context, 1, 1,
                                                 &pipeline->tex_constants);
    }

    /* Se dibuja por tramos contiguos de mezcla: un OMSetBlendState + un
     * DrawIndexed por tramo, sin cambiar el orden de los índices. Así el
     * estado cambia donde el original cambia grAlphaBlendFunction, y no por
     * triángulo ni por textura. Una malla viva siempre tiene al menos un
     * tramo: `gex_d3d11_mesh_create` añade uno ALPHA si la geometría no trae
     * tramos. */
    {
        UINT run;
        UINT total = 0;
        for (run = 0; run < mesh->run_count; run++) {
            const gex_geometry_run *r = &mesh->runs[run];
            ID3D11BlendState *state = pipeline->blend;
            UINT first;
            UINT count;

            if (r->blend < (uint8_t)GEX_BLEND_MODE_COUNT &&
                pipeline->blends[r->blend] != NULL) {
                state = pipeline->blends[r->blend];
            }
            if (r->index_offset > (size_t)UINT_MAX ||
                r->index_count > (size_t)UINT_MAX) {
                pipeline->last_hresult = E_INVALIDARG;
                return GEX_D3D11_PIPELINE_BAD_ARGUMENT;
            }
            first = (UINT)r->index_offset;
            count = (UINT)r->index_count;
            ID3D11DeviceContext_OMSetBlendState(pipeline->context, state, NULL,
                                                0xFFFFFFFFu);
            ID3D11DeviceContext_DrawIndexed(pipeline->context, count, first, 0);
            total += count;
        }
        pipeline->draw_calls += mesh->run_count;
        pipeline->triangles_drawn += total / 3u;
    }

    pipeline->last_hresult = S_OK;
    return GEX_D3D11_PIPELINE_OK;
}

const char *gex_d3d11_pipeline_status_name(gex_d3d11_pipeline_status status)
{
    switch (status) {
    case GEX_D3D11_PIPELINE_OK:
        return "ok";
    case GEX_D3D11_PIPELINE_BAD_ARGUMENT:
        return "bad argument";
    case GEX_D3D11_PIPELINE_BAD_STATE:
        return "bad state";
    case GEX_D3D11_PIPELINE_BAD_DEVICE:
        return "bad device";
    case GEX_D3D11_PIPELINE_NO_COMPILER:
        return "shader compiler unavailable";
    case GEX_D3D11_PIPELINE_SHADER_FAILED:
        return "shader compilation failed";
    case GEX_D3D11_PIPELINE_CREATE_FAILED:
        return "resource creation failed";
    case GEX_D3D11_PIPELINE_NO_GEOMETRY:
        return "no geometry";
    }
    return "unknown";
}

#endif /* _WIN32 */
