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
    UpdateWindow(item_->window());
#ifdef _DEBUG
    ResynchronizeTimer();
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

void VideoFrameTextureNode::AddVideoFrames(std::vector<QSharedPointer<VideoFrame>> &&frame)
{
    video_frames_.insert(video_frames_.end(), std::make_move_iterator(frame.begin()), std::make_move_iterator(frame.end()));
}

void VideoFrameTextureNode::Synchronize(QQuickItem *item)
{
    bool needs_new = false;

    if (item != item_)
    {
        item_ = item;
    }
    QQuickWindow *window = item_->window();
    if (window != window_)
    {
        UpdateWindow(window);
    }

    const QSize new_size = (rect().size() * dpr_).toSize();

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

        for (size_t i = 0; i < rendered_texture_queue_.size(); ++i)
        {
            rendered_texture_queue_[i].do_not_recycle = true;
        }
        for (size_t i = 0; i < used_texture_queue_.size(); ++i)
        {
            used_texture_queue_[i].do_not_recycle = true;
        }
        empty_texture_queue_.clear();

        Q_ASSERT(rendered_texture_queue_.size() + used_texture_queue_.size() <= kQueueSize);
        if (rendered_texture_queue_.size() + used_texture_queue_.size() < kQueueSize)
            NewTextureItem(kQueueSize - rendered_texture_queue_.size() - used_texture_queue_.size());

        if (!texture()) //Have to assign a texture if there isn't any
        {
            auto &empty_texture = empty_texture_queue_.front();
            setTexture(empty_texture.texture_qsg.get());
            used_texture_queue_.push_back(std::move(empty_texture));
            empty_texture_queue_.erase(empty_texture_queue_.begin());
        }
    }

    auto current_time = PlaybackClock::now();

    auto present_time_limit = current_time + std::chrono::microseconds(100);
    QSGTexture *next_texture_qsg = nullptr;
    while (!rendered_texture_queue_.empty() && rendered_texture_queue_.front().present_time <= present_time_limit)
    {
#ifdef _DEBUG
        frames_per_second_ += 1;
#endif
        auto &next_texture = rendered_texture_queue_.front();
        next_texture_qsg = next_texture.texture_qsg.get();
        used_texture_queue_.push_back(std::move(next_texture));
        rendered_texture_queue_.erase(rendered_texture_queue_.begin());
    }

    if (next_texture_qsg != nullptr)
    {
        setTexture(next_texture_qsg);
#ifdef _DEBUG
        texture_updates_per_second_ += 1;
        if (current_time - last_texture_change_time_ > max_texture_diff_time_)
            max_texture_diff_time_ = current_time - last_texture_change_time_;
        if (current_time - last_texture_change_time_ < min_texture_diff_time_)
            min_texture_diff_time_ = current_time - last_texture_change_time_;
        last_texture_change_time_ = current_time;
#endif
    }

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
        qDebug("%d textures rendered", rendered_texture_queue_.size());
        qDebug("%d textures used", used_texture_queue_.size());
        qDebug("%d textures empty", empty_texture_queue_.size());
        qDebug("%d frames in pending queue", video_frames_.size());
        qDebug("%d fps from source", frames_per_second_);
        qDebug("%d fps used", renders_per_second_);
        qDebug("%d texture updates", texture_updates_per_second_);
        qDebug("%lldus max diff (frame to frame)", std::chrono::duration_cast<std::chrono::microseconds>(max_diff_time_).count());
        qDebug("%lldus min diff (frame to frame)", std::chrono::duration_cast<std::chrono::microseconds>(min_diff_time_).count());
        qDebug("%lldus max diff (texture to texture)", std::chrono::duration_cast<std::chrono::microseconds>(max_texture_diff_time_).count());
        qDebug("%lldus min diff (texture to texture)", std::chrono::duration_cast<std::chrono::microseconds>(min_texture_diff_time_).count());
        max_diff_time_ = max_texture_diff_time_ = PlaybackClock::duration(0);
        min_diff_time_ = min_texture_diff_time_ = std::chrono::seconds(10);
        frames_per_second_ = renders_per_second_ = texture_updates_per_second_ = 0;
    }
#endif
}

void VideoFrameTextureNode::Render()
{
#ifdef _DEBUG
    if (rendered_texture_queue_.empty())
        ResynchronizeTimer();
#endif
    while (used_texture_queue_.size() > kUsedQueueSize)
    {
        auto &used_texture = used_texture_queue_.front();
        if (used_texture.do_not_recycle)
        {
            used_texture.texture.Reset();
            used_texture.texture_share_handle = nullptr;
            used_texture.texture_qsg.reset();
            used_texture.do_not_recycle = false;
            NewTextureItem(1);
        }
        else
        {
            empty_texture_queue_.push_back(std::move(used_texture));
        }
        used_texture_queue_.erase(used_texture_queue_.begin());
    }
    while (!video_frames_.empty() && !empty_texture_queue_.empty())
    {
        bool success = RenderFrame(empty_texture_queue_.front());
        if (!success)
            break;
        rendered_texture_queue_.push_back(std::move(empty_texture_queue_.front()));
        empty_texture_queue_.erase(empty_texture_queue_.begin());
    }
}

void VideoFrameTextureNode::UpdateScreen()
{
    if (window_->effectiveDevicePixelRatio() != dpr_)
        item_->update();
}

void VideoFrameTextureNode::UpdateWindow(QQuickWindow *new_window)
{
    if (window_)
    {
        disconnect(window_, &QQuickWindow::frameSwapped, this, &VideoFrameTextureNode::Render);
        disconnect(window_, &QQuickWindow::screenChanged, this, &VideoFrameTextureNode::UpdateScreen);
    }
    window_ = new_window;
    if (window_)
    {
        dpr_ = window_->effectiveDevicePixelRatio();
        connect(window_, &QQuickWindow::frameSwapped, this, &VideoFrameTextureNode::Render, Qt::DirectConnection);
        connect(window_, &QQuickWindow::screenChanged, this, &VideoFrameTextureNode::UpdateScreen);
    }
}

void VideoFrameTextureNode::ResynchronizeTimer()
{
#ifdef _DEBUG
    last_frame_time_ = last_texture_change_time_ = last_second_ = PlaybackClock::now();
    max_diff_time_ = max_texture_diff_time_ = PlaybackClock::duration(0);
    min_diff_time_ = min_texture_diff_time_ = std::chrono::seconds(10);
#endif
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
    video_frames_.erase(video_frames_.begin());

    HRESULT hr;
    D3D11SharedResource *shared_resource = D3D11SharedResource::resource;
    ID3D11Device *device = shared_resource->device;

    do
    {
        target.present_time = current_video_frame->present_time;

        float horizontal_limit, vertical_limit;
        float src_aspect_ratio = (float)current_video_frame->size.width() / current_video_frame->size.height();
        if (src_aspect_ratio == aspect_ratio_)
            horizontal_limit = vertical_limit = 1.0f;
        else if (src_aspect_ratio > aspect_ratio_)
            horizontal_limit = 1.0f, vertical_limit = (float)(size_.width() * current_video_frame->size.height()) / (current_video_frame->size.width() * size_.height());
        else
            horizontal_limit = (float)(size_.height() * current_video_frame->size.width()) / (current_video_frame->size.height() * size_.width()), vertical_limit = 1.0f;

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
