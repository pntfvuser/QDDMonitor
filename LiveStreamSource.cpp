#include "pch.h"
#include "LiveStreamSource.h"

#include "LiveStreamView.h"
#include "D3D11SharedResource.h"

static constexpr AVPixelFormat hw_pix_fmt = AV_PIX_FMT_D3D11;

enum AVPixelFormat get_hw_format(AVCodecContext *ctx,
    const enum AVPixelFormat *pix_fmts)
{
    Q_UNUSED(ctx);
    const enum AVPixelFormat *p;

    //See if there is hw pixel format first
    for (p = pix_fmts; *p != -1; p++) {
        if (*p == hw_pix_fmt)
            return *p;
    }

    //Return preference of decoder otherwise
    return *pix_fmts;
}

LiveStreamSource::LiveStreamSource(QObject *parent)
    :QObject(parent)
{
    connect(this, &LiveStreamSource::queueNextVideoFrameTick, this, &LiveStreamSource::onNextVideoFrameTick, Qt::QueuedConnection);

    constexpr AVHWDeviceType type = AV_HWDEVICE_TYPE_D3D11VA;
    int i, ret;

    if (avformat_open_input(input_ctx.GetAddressOf(), "E:\\Home\\Documents\\Qt\\QDDMonitor\\testsrc.mp4", NULL, NULL) != 0)
    {
        fprintf(stderr, "Cannot open input file '%s'\n", "testsrc.mp4");
        return;
    }

    if (avformat_find_stream_info(input_ctx.Get(), NULL) < 0)
    {
        fprintf(stderr, "Cannot find input stream information.\n");
        return;
    }

    /* find the video stream information */
    AVCodec *decoder = nullptr;
    ret = av_find_best_stream(input_ctx.Get(), AVMEDIA_TYPE_VIDEO, -1, -1, &decoder, 0);
    if (ret < 0)
    {
        fprintf(stderr, "Cannot find a video stream in the input file\n");
        return;
    }
    video_stream_index = ret;

    /*
    ret = av_find_best_stream(input_ctx, AVMEDIA_TYPE_AUDIO, -1, -1, &decoder, 0);
    if (ret < 0)
{
        fprintf(stderr, "Cannot find a audio stream in the input file\n");
        return;
    }
    audio_stream_index = ret;
    */

    AVStream *video_stream = input_ctx->streams[video_stream_index];
    static constexpr auto clock_tick_per_second = std::chrono::duration_cast<PlaybackClock::duration>(std::chrono::seconds(1)).count();
    auto frame_rate_tick = (video_stream->r_frame_rate.den * clock_tick_per_second + video_stream->r_frame_rate.num - 1) / video_stream->r_frame_rate.num;
    frame_rate_ = PlaybackClock::duration(frame_rate_tick);
    if (frame_rate_.count() <= 0)
        frame_rate_ = PlaybackClock::duration(1);

    for (i = 0;; i++)
    {
        const AVCodecHWConfig *config = avcodec_get_hw_config(decoder, i);
        if (!config)
        {
            fprintf(stderr, "Decoder %s does not support device type %s.\n", decoder->name, av_hwdevice_get_type_name(type));
            return;
        }
        if (config->methods & AV_CODEC_HW_CONFIG_METHOD_HW_DEVICE_CTX && config->device_type == type && hw_pix_fmt == config->pix_fmt)
        {
            break;
        }
    }

    if (!(video_decoder_ctx = avcodec_alloc_context3(decoder)))
        return;

    if (avcodec_parameters_to_context(video_decoder_ctx.Get(), video_stream->codecpar) < 0)
        return;

    video_decoder_ctx->hw_device_ctx = av_buffer_ref(D3D11SharedResource::resource->hw_device_ctx_obj.Get());

    if ((ret = avcodec_open2(video_decoder_ctx.Get(), decoder, NULL)) < 0)
    {
        fprintf(stderr, "Failed to open codec for stream #%u\n", video_stream_index);
        return;
    }

    if (video_decoder_ctx->pix_fmt != hw_pix_fmt)
    {
        sws_context_ = sws_getContext(video_decoder_ctx->width, video_decoder_ctx->height, video_decoder_ctx->pix_fmt,
                                      video_decoder_ctx->width, video_decoder_ctx->height, AV_PIX_FMT_RGB0,
                                      SWS_BICUBLIN | SWS_BITEXACT, nullptr, nullptr, nullptr);
        if (!sws_context_)
        {
            fprintf(stderr, "Failed to open swscale context #%u\n", video_stream_index);
            return;
        }
    }
}

void LiveStreamSource::start()
{
    next_frame_time_ = PlaybackClock::now();
    next_frame_offset_ = PlaybackClock::duration(0);
    emit newMedia();
    SetUpNextVideoFrameTick();
    debugMSTick();
}

void LiveStreamSource::onNextVideoFrameTick()
{
    ReceiveVideoFrame();
    SetUpNextVideoFrameTick();
}

void LiveStreamSource::debugMSTick()
{
    emit debugRefreshSignal();
    QTimer::singleShot(4, this, &LiveStreamSource::debugMSTick);
}

void LiveStreamSource::SendData()
{
    int ret;

    if ((ret = av_read_frame(input_ctx.Get(), &packet)) < 0)
        return;
    while (packet.stream_index != video_stream_index)
    {
        av_packet_unref(&packet);
        if ((ret = av_read_frame(input_ctx.Get(), &packet)) < 0)
            return;
    }

    ret = avcodec_send_packet(video_decoder_ctx.Get(), &packet);
    av_packet_unref(&packet);
    if (ret < 0)
    {
        fprintf(stderr, "Error during decoding\n");
        return;
    }
}

void LiveStreamSource::ReceiveVideoFrame()
{
    AVFrameObject frame;
    int ret = 0;

    if (!(frame = av_frame_alloc()))
    {
        fprintf(stderr, "Can not alloc frame\n");
        return;
    }

    ret = avcodec_receive_frame(video_decoder_ctx.Get(), frame.Get());
    if (ret == AVERROR(EAGAIN))
    {
        do
        {
            SendData();
            ret = avcodec_receive_frame(video_decoder_ctx.Get(), frame.Get());
        } while (ret == AVERROR(EAGAIN));
    }
    if (ret == AVERROR_EOF)
    {
        return;
    }
    else if (ret < 0)
    {
        fprintf(stderr, "Error while decoding\n");
        return;
    }

    static const QMetaMethod newVideoFrameSignal = QMetaMethod::fromSignal(&LiveStreamSource::newVideoFrame);
    if (!isSignalConnected(newVideoFrameSignal))
    {
        return; //Silently discard
    }

    if (frame->format == hw_pix_fmt) {
        D3D11_TEXTURE2D_DESC texture_desc;

        ID3D11Texture2D *decoded_texture = reinterpret_cast<ID3D11Texture2D *>(frame->data[0]);
        const int decoded_texture_index = reinterpret_cast<uintptr_t>(frame->data[1]);
        decoded_texture->GetDesc(&texture_desc);
        int width = texture_desc.Width;
        int height = texture_desc.Height;
        if (width <= 0 || height <= 0)
            return;

        ZeroMemory(&texture_desc, sizeof(texture_desc));
        texture_desc.Format = DXGI_FORMAT_NV12;              // Pixel format
        texture_desc.Width = width;                          // Width of the video frames
        texture_desc.Height = height;                        // Height of the video frames
        texture_desc.ArraySize = 1;                          // Number of textures in the array
        texture_desc.MipLevels = 1;                          // Number of miplevels in each texture
        texture_desc.BindFlags = D3D11_BIND_SHADER_RESOURCE; // We read from this texture in the shader
        texture_desc.Usage = D3D11_USAGE_DEFAULT;
        texture_desc.MiscFlags = 0;
        texture_desc.CPUAccessFlags = 0;
        texture_desc.SampleDesc.Count = 1;

        ComPtr<ID3D11Texture2D> nv12_texture = nullptr;
        D3D11SharedResource *shared_resource = D3D11SharedResource::resource;
        HRESULT hr;
        hr = shared_resource->device->CreateTexture2D(&texture_desc, nullptr, &nv12_texture);
        if (FAILED(hr))
        {
            fprintf(stderr, "Error while creating nv12_texture\n");
            return;
        }

        AVD3D11VADeviceContextUniqueLock lock(shared_resource->d3d11_device_ctx);
        shared_resource->device_context->CopySubresourceRegion(nv12_texture.Get(), 0, 0, 0, 0, decoded_texture, decoded_texture_index, nullptr);
        lock.Unlock();

        QSharedPointer<VideoFrame> video_frame = QSharedPointer<VideoFrame>::create();
        //video_frame->frame = std::move(frame);
        video_frame->frame_time = next_frame_time_ + next_frame_offset_ + kDecodeToRenderLatency;
        video_frame->frame_size = QSize(width, height);
        video_frame->texture = std::move(nv12_texture);
        video_frame->is_rgbx = false;

        Q_ASSERT(video_frame->texture);
        emit newVideoFrame(video_frame);
    }
    else
    {
        Q_ASSERT(frame->format == video_decoder_ctx->pix_fmt);
        Q_ASSERT(frame->width == video_decoder_ctx->width);
        Q_ASSERT(frame->height == video_decoder_ctx->height);

        int width = frame->width, height = frame->height;
        int dst_line_size = sizeof(uint32_t) * width;
        int dst_size = dst_line_size * height;
        std::unique_ptr<uint8_t[]> dst_buffer = std::make_unique<uint8_t[]>(dst_size);
        uint8_t *dst_data[3] = { dst_buffer.get(), nullptr, nullptr };
        int dst_stride[3] = { dst_line_size, 0, 0 };
        ret = sws_scale(sws_context_.Get(), frame->data, frame->linesize, 0, height, dst_data, dst_stride);
        if (ret < 0)
        {
            fprintf(stderr, "Error while color format converting\n");
            return;
        }

        D3D11_TEXTURE2D_DESC texture_desc;
        ZeroMemory(&texture_desc, sizeof(texture_desc));
        texture_desc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;    // Pixel format
        texture_desc.Width = width;                          // Width of the video frames
        texture_desc.Height = height;                        // Height of the video frames
        texture_desc.ArraySize = 1;                          // Number of textures in the array
        texture_desc.MipLevels = 1;                          // Number of miplevels in each texture
        texture_desc.BindFlags = D3D11_BIND_SHADER_RESOURCE; // We read from this texture in the shader
        texture_desc.Usage = D3D11_USAGE_IMMUTABLE;
        texture_desc.MiscFlags = 0;
        texture_desc.CPUAccessFlags = 0;
        texture_desc.SampleDesc.Count = 1;
        D3D11_SUBRESOURCE_DATA texture_init_data;
        ZeroMemory(&texture_init_data, sizeof(texture_init_data));
        texture_init_data.pSysMem = dst_buffer.get();
        texture_init_data.SysMemPitch = dst_line_size;

        ComPtr<ID3D11Texture2D> rgbx_texture = nullptr;
        D3D11SharedResource *shared_resource = D3D11SharedResource::resource;
        HRESULT hr;
        hr = shared_resource->device->CreateTexture2D(&texture_desc, &texture_init_data, &rgbx_texture);
        if (FAILED(hr))
        {
            fprintf(stderr, "Error while creating rgb_texture\n");
            return;
        }

        AVD3D11VADeviceContextUniqueLock lock(shared_resource->d3d11_device_ctx);
        shared_resource->device_context->Flush();
        lock.Unlock();

        QSharedPointer<VideoFrame> video_frame = QSharedPointer<VideoFrame>::create();
        //video_frame->frame = std::move(frame);
        video_frame->frame_time = next_frame_time_ + next_frame_offset_ + kDecodeToRenderLatency;
        video_frame->frame_size = QSize(width, height);
        video_frame->texture = std::move(rgbx_texture);
        video_frame->is_rgbx = true;

        Q_ASSERT(video_frame->texture);
        emit newVideoFrame(video_frame);
    }
}

void LiveStreamSource::SetUpNextVideoFrameTick()
{
    frames_per_second_ += 1;

    next_frame_offset_ += frame_rate_;
    if (next_frame_offset_ > std::chrono::seconds(1))
    {
        next_frame_offset_ = PlaybackClock::duration(0);
        next_frame_time_ += std::chrono::seconds(1);
        //qDebug((std::to_string(frames_per_second_) + "fps").c_str());
        frames_per_second_ = 0;
    }
    auto next_frame_tick_time = next_frame_time_ + next_frame_offset_;
    auto current_time = PlaybackClock::now();
    auto sleep_time = next_frame_tick_time - current_time;
    auto sleep_ms = std::chrono::duration_cast<std::chrono::milliseconds>(sleep_time).count();
    if (sleep_ms <= 0)
    {
        emit queueNextVideoFrameTick();
        return;
    }
    QTimer::singleShot(sleep_ms, this, &LiveStreamSource::onNextVideoFrameTick);
}
