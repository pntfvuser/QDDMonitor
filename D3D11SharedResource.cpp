#include "pch.h"
#include "D3D11SharedResource.h"

#include "NullVertexShader.h"
#include "RGBXPixelShader.h"
#include "NV12PixelShader.h"

D3D11SharedResource *D3D11SharedResource::resource = nullptr;

using DirectX::XMFLOAT2;

D3D11SharedResource::D3D11SharedResource()
{
    int err = 0;

    AVDictionary *dic = nullptr;
#ifdef _DEBUG
    av_dict_set(&dic, "debug", "true", 0);
#endif

    AVBufferRef *hw_device_ctx;
    if ((err = av_hwdevice_ctx_create(&hw_device_ctx, AV_HWDEVICE_TYPE_D3D11VA, NULL, dic, 0)) < 0)
    {
        throw std::runtime_error("Failed to create specified HW device, code " + std::to_string(err));
    }
    hw_device_ctx_obj = hw_device_ctx;
    if (dic)
        av_dict_free(&dic);

    AVHWDeviceContext *device_ctx = reinterpret_cast<AVHWDeviceContext *>(hw_device_ctx_obj->data);
    d3d11_device_ctx = reinterpret_cast<AVD3D11VADeviceContext *>(device_ctx->hwctx);
    device = d3d11_device_ctx->device;
    device_context = d3d11_device_ctx->device_context;
    video_device = d3d11_device_ctx->video_device;
    video_context = d3d11_device_ctx->video_context;

    HRESULT hr;

    D3D11_SAMPLER_DESC sampler_desc = CD3D11_SAMPLER_DESC(CD3D11_DEFAULT());
    hr = device->CreateSamplerState(&sampler_desc, &sampler_linear);
    if (FAILED(hr))
    {
        throw std::runtime_error("Failed to create sampler state, code " + std::to_string(hr));
    }

    UINT nullvs_size = ARRAYSIZE(g_NullVertexShader);
    hr = device->CreateVertexShader(g_NullVertexShader, nullvs_size, nullptr, &null_vertex_shader);
    if (FAILED(hr))
    {
        throw std::runtime_error("Failed to create null vertex shader, code " + std::to_string(hr));
    }

    UINT rgbxps_size = ARRAYSIZE(g_RGBXPixelShader);
    hr = device->CreatePixelShader(g_RGBXPixelShader, rgbxps_size, nullptr, &rgbx_pixel_shader);
    if (FAILED(hr))
    {
        throw std::runtime_error("Failed to create null pixel shader, code " + std::to_string(hr));
    }

    UINT nv12ps_size = ARRAYSIZE(g_NV12PixelShader);
    hr = device->CreatePixelShader(g_NV12PixelShader, nv12ps_size, nullptr, &nv12_pixel_shader);
    if (FAILED(hr))
    {
        throw std::runtime_error("Failed to create nv12 pixel shader, code " + std::to_string(hr));
    }

    static constexpr D3D11_INPUT_ELEMENT_DESC layout[] =
    {
        { "POSITION", 0, DXGI_FORMAT_R32G32_FLOAT,    0, 0, D3D11_INPUT_PER_VERTEX_DATA, 0 },
        { "TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT,    0, 8, D3D11_INPUT_PER_VERTEX_DATA, 0 },
    };
    hr = device->CreateInputLayout(layout, std::extent<decltype(layout)>::value, g_NullVertexShader, nullvs_size, &input_layout);
    if (FAILED(hr))
    {
        throw std::runtime_error("Failed to create input layout, code " + std::to_string(hr));
    }
}

D3D11SharedResource::~D3D11SharedResource()
{
}
