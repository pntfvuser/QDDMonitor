#ifndef D3D11SHAREDRESOURCE_H
#define D3D11SHAREDRESOURCE_H

#include "AVObjectWrapper.h"

struct D3D11Vertex
{
    DirectX::XMFLOAT2 Pos;
    DirectX::XMFLOAT2 TexCoord;
};

struct D3D11SharedResource
{
    static D3D11SharedResource *resource;

    AVBufferRefObject hw_device_ctx_obj;

    //Objects managed by hw_device_ctx_obj

    AVD3D11VADeviceContext *d3d11_device_ctx = nullptr;
    ID3D11Device *device = nullptr;
    ID3D11DeviceContext *device_context = nullptr;
    ID3D11VideoDevice *video_device = nullptr;
    ID3D11VideoContext *video_context = nullptr;

    //Objects managed directly

    ComPtr<ID3D11SamplerState> sampler_linear = nullptr;
    ComPtr<ID3D11VertexShader> null_vertex_shader = nullptr;
    ComPtr<ID3D11PixelShader> rgbx_pixel_shader = nullptr;
    ComPtr<ID3D11PixelShader> yuvj444p_pixel_shader = nullptr;
    ComPtr<ID3D11PixelShader> nv12_pixel_shader = nullptr;
    ComPtr<ID3D11InputLayout> input_layout = nullptr;

    D3D11SharedResource();
    ~D3D11SharedResource();
};

class AVD3D11VADeviceContextLockGuard
{
public:
    AVD3D11VADeviceContextLockGuard(AVD3D11VADeviceContext *context) :context_(context) { context_->lock(context_->lock_ctx); }
    AVD3D11VADeviceContextLockGuard(const AVD3D11VADeviceContextLockGuard &) = delete;
    AVD3D11VADeviceContextLockGuard(AVD3D11VADeviceContextLockGuard &&) = delete;
    ~AVD3D11VADeviceContextLockGuard() { context_->unlock(context_->lock_ctx); }
private:
    AVD3D11VADeviceContext *context_;
};

struct AVD3D11VADeviceContextUniqueLock
{
public:
    AVD3D11VADeviceContextUniqueLock(AVD3D11VADeviceContext *context) :context_(context) { context_->lock(context_->lock_ctx); locked_ = true; }
    AVD3D11VADeviceContextUniqueLock(const AVD3D11VADeviceContextLockGuard &) = delete;
    AVD3D11VADeviceContextUniqueLock(AVD3D11VADeviceContextLockGuard &&) = delete;
    ~AVD3D11VADeviceContextUniqueLock() { if (locked_) context_->unlock(context_->lock_ctx); }

    void Lock() { if (!locked_) { context_->lock(context_->lock_ctx); locked_ = true; } }
    void Unlock() { if (locked_) { context_->unlock(context_->lock_ctx); locked_ = false; } }
private:
    AVD3D11VADeviceContext *context_;
    bool locked_ = false;
};

#endif // D3D11SHAREDRESOURCE_H
