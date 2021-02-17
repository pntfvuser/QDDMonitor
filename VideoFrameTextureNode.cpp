#include "pch.h"
#include "VideoFrameTextureNode.h"

#ifdef _WIN32
#include "D3D11SharedResource.h"
#endif

VideoFrameTextureNode::TextureItem::TextureItem(QQuickWindow *window, ID3D11Device *device, const D3D11_TEXTURE2D_DESC &desc, const QSize &size)
{
    HRESULT hr;
    hr = device->CreateTexture2D(&desc, NULL, &texture);
    if (SUCCEEDED(hr))
    {
        ComPtr<IDXGIResource> dxgi_resource = nullptr;
        hr = texture.As(&dxgi_resource);
        Q_ASSERT(SUCCEEDED(hr));
        hr = dxgi_resource->GetSharedHandle(&texture_share_handle);
        Q_ASSERT(SUCCEEDED(hr));

        texture_qsg = std::unique_ptr<QSGTexture>(window->createTextureFromNativeObject(QQuickWindow::NativeObjectTexture, texture.GetAddressOf(), 0, size, QQuickWindow::TextureHasAlphaChannel));
    }
    else
    {
        texture = nullptr;
        texture_share_handle = nullptr;
        texture_qsg.reset();
    }
}

VideoFrameTextureNode::VideoFrameTextureNode(QQuickItem *item)
    :item_(item)
{
    window_ = item_->window();
    dpr_ = window_->effectiveDevicePixelRatio();
    connect(window_, &QQuickWindow::frameSwapped, this, &VideoFrameTextureNode::Render, Qt::DirectConnection);
    connect(window_, &QQuickWindow::screenChanged, this, &VideoFrameTextureNode::UpdateScreen);

#ifdef _DEBUG
    last_frame_time_ = PlaybackClock::now();
    max_diff_time_ = PlaybackClock::duration(0);
    min_diff_time_ = std::chrono::seconds(10);
#endif
}

VideoFrameTextureNode::~VideoFrameTextureNode()
{
}

QSGTexture *VideoFrameTextureNode::texture() const
{
    return QSGSimpleTextureNode::texture();
}

void VideoFrameTextureNode::AddVideoFrame(const QSharedPointer<VideoFrame> &frame)
{
    video_frames_.push_back(frame);
}

void VideoFrameTextureNode::ResynchronizeTimer()
{
#ifdef _DEBUG
    last_frame_time_ = last_second_ = PlaybackClock::now();
    max_diff_time_ = PlaybackClock::duration(0);
    min_diff_time_ = std::chrono::seconds(10);
#endif
}

void VideoFrameTextureNode::Synchronize()
{
    const QSize new_size = (rect().size() * dpr_).toSize();
    bool needs_new = false;

    if (!texture())
        needs_new = true;

    if (new_size != size_)
    {
        needs_new = true;
        size_ = new_size;
        if (size_.width() <= 0 || size_.height() <= 0)
            aspect_ratio_ = 0;
        else
            aspect_ratio_ = (float)size_.width() / size_.height();
    }

    if (needs_new)
    {
        QSGRendererInterface *rif = window_->rendererInterface();
        device_ = reinterpret_cast<ID3D11Device *>(rif->getResource(window_, QSGRendererInterface::DeviceResource));
        Q_ASSERT(device_);

        for (int i = 0; i < rendered_texture_queue_.size(); ++i)
        {
            rendered_texture_queue_[i].do_not_recycle = true;
        }
        if (!empty_texture_queue_.empty())
            garbage_texture_ = std::move(empty_texture_queue_.back()); //Keep last used texture
        empty_texture_queue_.clear();
        NewTextureItem(std::max(0, kQueueSize - rendered_texture_queue_.size()));

        if (!texture()) //Have to assign a texture
        {
            auto &empty_texture = empty_texture_queue_.front();
            setTexture(empty_texture.texture_qsg.get());
            rendered_texture_queue_.push_back(std::move(empty_texture));
            empty_texture_queue_.pop_front();
        }
    }

    if (playing_)
    {
        QSGTexture *next_texture_qsg = nullptr;

        auto current_time = PlaybackClock::now();
        while (!rendered_texture_queue_.empty() && rendered_texture_queue_.front().frame_time <= current_time)
        {
#ifdef _DEBUG
            frames_per_second_ += 1;
#endif
            auto &next_texture = rendered_texture_queue_.front();
            next_texture_qsg = next_texture.texture_qsg.get();
            if (next_texture.do_not_recycle)
            {
                NewTextureItem(1);
                garbage_texture_ = std::move(next_texture);
            }
            else
            {
                empty_texture_queue_.push_back(std::move(next_texture));
                if (garbage_texture_.do_not_recycle)
                {
                    garbage_texture_.texture.Reset();
                    garbage_texture_.texture_share_handle = nullptr;
                    garbage_texture_.texture_qsg.reset();
                    garbage_texture_.do_not_recycle = false;
                }
            }
            rendered_texture_queue_.pop_front();
        }

        if (next_texture_qsg != nullptr)
            setTexture(next_texture_qsg);

#ifdef _DEBUG
        renders_per_second_ += 1;
        if (current_time - last_frame_time_ > max_diff_time_)
            max_diff_time_ = current_time - last_frame_time_;
        if (current_time - last_frame_time_ < min_diff_time_)
            min_diff_time_ = current_time - last_frame_time_;
        last_frame_time_ = current_time;
        if (current_time - last_second_ > std::chrono::seconds(1))
        {
            last_second_ += std::chrono::seconds(1);
            qDebug((std::to_string(rendered_texture_queue_.size()) + " frames rendered").c_str());
            qDebug((std::to_string(video_frames_.size()) + " frames pending").c_str());
            qDebug((std::to_string(frames_per_second_) + " sfps").c_str());
            qDebug((std::to_string(renders_per_second_) + " fps").c_str());
            qDebug((std::to_string(std::chrono::duration_cast<std::chrono::microseconds>(max_diff_time_).count()) + "us max diff").c_str());
            qDebug((std::to_string(std::chrono::duration_cast<std::chrono::microseconds>(min_diff_time_).count()) + "us min diff").c_str());
            max_diff_time_ = PlaybackClock::duration(0);
            min_diff_time_ = std::chrono::seconds(10);
            frames_per_second_ = renders_per_second_ = 0;
        }
#endif
    }
}

void VideoFrameTextureNode::Render()
{
    while (!video_frames_.empty() && !empty_texture_queue_.empty())
    {
        bool success = RenderFrame(empty_texture_queue_.front());
        if (!success)
            break;
        rendered_texture_queue_.push_back(std::move(empty_texture_queue_.front()));
        empty_texture_queue_.pop_front();
    }
    if (!playing_ && rendered_texture_queue_.size() >= kQueueStartPlaySize)
    {
        playing_ = true;
        ResynchronizeTimer();
    }
}

void VideoFrameTextureNode::UpdateScreen()
{
    if (window_->effectiveDevicePixelRatio() != dpr_)
        item_->update();
}

void VideoFrameTextureNode::NewTextureItem(int count)
{
    D3D11_TEXTURE2D_DESC desc;
    ZeroMemory(&desc, sizeof(desc));
    desc.Width = size_.width();
    desc.Height = size_.height();
    desc.MipLevels = desc.ArraySize = 1;
    desc.Format = kTextureFormat;
    desc.SampleDesc.Count = 1;
    desc.MiscFlags = D3D11_RESOURCE_MISC_SHARED; // D3D11_RESOURCE_MISC_SHARED_KEYEDMUTEX
    desc.Usage = D3D11_USAGE_DEFAULT;
    desc.BindFlags = D3D11_BIND_RENDER_TARGET | D3D11_BIND_SHADER_RESOURCE;
    desc.CPUAccessFlags = 0;

    for (int i = 0; i < count; ++i)
    {
        empty_texture_queue_.push_back(TextureItem(window_, device_, desc, size_));
    }
}

bool VideoFrameTextureNode::RenderFrame(TextureItem &target)
{
    if (video_frames_.empty())
        return false;
    if (aspect_ratio_ == 0)
        return false;
    QSharedPointer<VideoFrame> current_video_frame = std::move(video_frames_.front());
    video_frames_.pop_front();

    HRESULT hr;
    D3D11SharedResource *shared_resource = D3D11SharedResource::resource;
    ID3D11Device *device = shared_resource->device;

    do
    {
        target.frame_time = current_video_frame->frame_time;

        float horizontal_limit, vertical_limit;
        float src_aspect_ratio = (float)current_video_frame->frame_size.width() / current_video_frame->frame_size.height();
        if (src_aspect_ratio == aspect_ratio_)
            horizontal_limit = vertical_limit = 1.0f;
        else if (src_aspect_ratio > aspect_ratio_)
            horizontal_limit = 1.0f, vertical_limit = (float)(size_.width() * current_video_frame->frame_size.height()) / (current_video_frame->frame_size.width() * size_.height());
        else
            horizontal_limit = (float)(size_.height() * current_video_frame->frame_size.width()) / (current_video_frame->frame_size.height() * size_.width()), vertical_limit = 1.0f;

        ID3D11Texture2D *src_texture = current_video_frame->texture.Get();
        ComPtr<ID3D11Texture2D> dst_texture = nullptr;
        ComPtr<ID3D11Buffer> vertex_buffer = nullptr;
        ComPtr<ID3D11RenderTargetView> render_target_view = nullptr;
        Q_ASSERT(src_texture);

        UINT stride = sizeof(D3D11Vertex);
        UINT offset = 0;
        FLOAT blend_factor[4] = {0.0f, 0.0f, 0.0f, 0.0f};
        FLOAT clear_color[4] = {0.0f, 0.0f, 0.0f, 0.0f};

        hr = device->OpenSharedResource(target.texture_share_handle, __uuidof(ID3D11Texture2D), &dst_texture);
        if (FAILED(hr))
        {
            break;
        }

        D3D11Vertex vertices[] =
        {
            { DirectX::XMFLOAT2(-horizontal_limit, -vertical_limit), DirectX::XMFLOAT2(0.0f, 1.0f) },
            { DirectX::XMFLOAT2(-horizontal_limit, vertical_limit) , DirectX::XMFLOAT2(0.0f, 0.0f) },
            { DirectX::XMFLOAT2(horizontal_limit, -vertical_limit) , DirectX::XMFLOAT2(1.0f, 1.0f) },
            { DirectX::XMFLOAT2(horizontal_limit, -vertical_limit) , DirectX::XMFLOAT2(1.0f, 1.0f) },
            { DirectX::XMFLOAT2(-horizontal_limit, vertical_limit) , DirectX::XMFLOAT2(0.0f, 0.0f) },
            { DirectX::XMFLOAT2(horizontal_limit, vertical_limit)  , DirectX::XMFLOAT2(1.0f, 0.0f) },
        };
        static constexpr size_t num_of_vertices = std::extent<decltype(vertices)>::value;

        D3D11_BUFFER_DESC vertex_buffer_desc;
        ZeroMemory(&vertex_buffer_desc, sizeof(vertex_buffer_desc));
        vertex_buffer_desc.Usage = D3D11_USAGE_DEFAULT;
        vertex_buffer_desc.ByteWidth = sizeof(D3D11Vertex) * num_of_vertices;
        vertex_buffer_desc.BindFlags = D3D11_BIND_VERTEX_BUFFER;
        vertex_buffer_desc.CPUAccessFlags = 0;
        D3D11_SUBRESOURCE_DATA vertex_init_data;
        ZeroMemory(&vertex_init_data, sizeof(vertex_init_data));
        vertex_init_data.pSysMem = vertices;

        hr = device->CreateBuffer(&vertex_buffer_desc, &vertex_init_data, &vertex_buffer);
        if (FAILED(hr))
        {
            throw std::runtime_error("Failed to create vertex buffer, code " + std::to_string(hr));
        }

        D3D11_RENDER_TARGET_VIEW_DESC render_target_view_desc;
        ZeroMemory(&render_target_view_desc, sizeof(render_target_view_desc));
        render_target_view_desc.Format = kTextureFormat;
        render_target_view_desc.ViewDimension = D3D11_RTV_DIMENSION_TEXTURE2D;
        render_target_view_desc.Texture2D.MipSlice = 0;
        hr = device->CreateRenderTargetView(dst_texture.Get(), &render_target_view_desc, &render_target_view);
        if (FAILED(hr))
        {
            break;
        }

        D3D11_VIEWPORT viewport;
        viewport.TopLeftX = 0;
        viewport.TopLeftY = 0;
        viewport.Width = size_.width();
        viewport.Height = size_.height();
        viewport.MinDepth = 0;
        viewport.MaxDepth = 1;

        if (current_video_frame->is_rgbx)
        {
            ComPtr<ID3D11ShaderResourceView> rgbx_view = nullptr;

            D3D11_SHADER_RESOURCE_VIEW_DESC const rgbx_plane_desc = CD3D11_SHADER_RESOURCE_VIEW_DESC(
                src_texture,
                D3D11_SRV_DIMENSION_TEXTURE2D,
                DXGI_FORMAT_R8G8B8A8_UNORM
            );
            hr = device->CreateShaderResourceView(
                src_texture,
                &rgbx_plane_desc,
                &rgbx_view
            );
            if (FAILED(hr))
            {
                break;
            }

            ID3D11ShaderResourceView *texture_views[] = {
                rgbx_view.Get(),
            };

            ID3D11DeviceContext *device_context = shared_resource->device_context;
            AVD3D11VADeviceContextLockGuard lock(shared_resource->d3d11_device_ctx);

            device_context->ClearRenderTargetView(render_target_view.Get(), clear_color);
            device_context->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
            device_context->IASetInputLayout(shared_resource->input_layout.Get());
            device_context->IASetVertexBuffers(0, 1, vertex_buffer.GetAddressOf(), &stride, &offset);
            device_context->VSSetShader(shared_resource->null_vertex_shader.Get(), nullptr, 0);
            device_context->PSSetShader(shared_resource->rgbx_pixel_shader.Get(), nullptr, 0);
            device_context->PSSetSamplers(0, 1, shared_resource->sampler_linear.GetAddressOf());
            device_context->PSSetShaderResources(0, std::extent<decltype(texture_views)>::value, texture_views);
            device_context->RSSetViewports(1, &viewport);
            device_context->OMSetBlendState(nullptr, blend_factor, 0xffffffff);
            device_context->OMSetRenderTargets(1, render_target_view.GetAddressOf(), nullptr);
            device_context->Draw(num_of_vertices, 0);
        }
        else
        {
            ComPtr<ID3D11ShaderResourceView> luminance_view = nullptr;
            ComPtr<ID3D11ShaderResourceView> chrominance_view = nullptr;

            D3D11_SHADER_RESOURCE_VIEW_DESC const luminance_plane_desc = CD3D11_SHADER_RESOURCE_VIEW_DESC(
                src_texture,
                D3D11_SRV_DIMENSION_TEXTURE2D,
                DXGI_FORMAT_R8_UNORM
            );
            hr = device->CreateShaderResourceView(
                src_texture,
                &luminance_plane_desc,
                &luminance_view
            );
            if (FAILED(hr))
            {
                break;
            }

            D3D11_SHADER_RESOURCE_VIEW_DESC const chrominance_plane_desc = CD3D11_SHADER_RESOURCE_VIEW_DESC(
                src_texture,
                D3D11_SRV_DIMENSION_TEXTURE2D,
                DXGI_FORMAT_R8G8_UNORM
            );
            hr = device->CreateShaderResourceView(
                src_texture,
                &chrominance_plane_desc,
                &chrominance_view
            );
            if (FAILED(hr))
            {
                break;
            }

            ID3D11ShaderResourceView *texture_views[] = {
                luminance_view.Get(),
                chrominance_view.Get(),
            };

            ID3D11DeviceContext *device_context = shared_resource->device_context;
            AVD3D11VADeviceContextLockGuard lock(shared_resource->d3d11_device_ctx);

            device_context->ClearRenderTargetView(render_target_view.Get(), clear_color);
            device_context->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
            device_context->IASetInputLayout(shared_resource->input_layout.Get());
            device_context->IASetVertexBuffers(0, 1, vertex_buffer.GetAddressOf(), &stride, &offset);
            device_context->VSSetShader(shared_resource->null_vertex_shader.Get(), nullptr, 0);
            device_context->PSSetShader(shared_resource->nv12_pixel_shader.Get(), nullptr, 0);
            device_context->PSSetSamplers(0, 1, shared_resource->sampler_linear.GetAddressOf());
            device_context->PSSetShaderResources(0, std::extent<decltype(texture_views)>::value, texture_views);
            device_context->RSSetViewports(1, &viewport);
            device_context->OMSetBlendState(nullptr, blend_factor, 0xffffffff);
            device_context->OMSetRenderTargets(1, render_target_view.GetAddressOf(), nullptr);
            device_context->Draw(num_of_vertices, 0);
        }
    } while (false);

    return true;
}
