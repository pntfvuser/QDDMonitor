#include "pch.h"
#include "LiveStreamSource.h"

#include "LiveStreamView.h"
#include "D3D11SharedResource.h"

static constexpr AVPixelFormat hw_pix_fmt = AV_PIX_FMT_D3D11;

template <typename ToDuration>
static inline constexpr PlaybackClock::duration AVTimestampToDuration(int64_t timestamp, AVRational time_base)
{
    return ToDuration((timestamp * time_base.num * ToDuration::period::den) / (time_base.den * ToDuration::period::num));
}

template <typename Rep, typename Period>
static inline constexpr int64_t DurationToAVTimestamp(std::chrono::duration<Rep, Period> duration, AVRational time_base)
{
    return (duration.count() * Period::num * time_base.den) / (Period::den * time_base.num);
}

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
    connect(this, &LiveStreamSource::queuePushTick, this, &LiveStreamSource::onPushTick, Qt::QueuedConnection);

    constexpr AVHWDeviceType type = AV_HWDEVICE_TYPE_D3D11VA;
    int i, ret;

    if (avformat_open_input(input_ctx_.GetAddressOf(), "E:\\Home\\Documents\\Qt\\QDDMonitor\\testsrc.mp4", NULL, NULL) != 0)
    {
        qWarning("Cannot open input file '%s'", "testsrc.mp4");
        return;
    }

    if (avformat_find_stream_info(input_ctx_.Get(), NULL) < 0)
    {
        qWarning("Cannot find input stream information.");
        return;
    }

    /* find the video stream information */
    AVCodec *video_decoder = nullptr;
    ret = av_find_best_stream(input_ctx_.Get(), AVMEDIA_TYPE_VIDEO, -1, -1, &video_decoder, 0);
    if (ret < 0)
    {
        qWarning("Cannot find a video stream in the input file");
        return;
    }
    video_stream_index_ = ret;

    AVStream *video_stream = input_ctx_->streams[video_stream_index_];

    for (i = 0;; i++)
    {
        const AVCodecHWConfig *config = avcodec_get_hw_config(video_decoder, i);
        if (!config)
        {
            qWarning("Decoder %s does not support device type %s.", video_decoder->name, av_hwdevice_get_type_name(type));
            return;
        }
        if (config->methods & AV_CODEC_HW_CONFIG_METHOD_HW_DEVICE_CTX && config->device_type == type && hw_pix_fmt == config->pix_fmt)
        {
            break;
        }
    }

    if (!(video_decoder_ctx_ = avcodec_alloc_context3(video_decoder)))
    {
        qWarning("Failed to alloc codec for video stream #%u", ret);
        return;
    }

    if (avcodec_parameters_to_context(video_decoder_ctx_.Get(), video_stream->codecpar) < 0)
        return;

    video_decoder_ctx_->hw_device_ctx = av_buffer_ref(D3D11SharedResource::resource->hw_device_ctx_obj.Get());

    if ((ret = avcodec_open2(video_decoder_ctx_.Get(), video_decoder, NULL)) < 0)
    {
        qWarning("Failed to open codec for video stream #%u", ret);
        return;
    }

    if (video_decoder_ctx_->pix_fmt != hw_pix_fmt)
    {
        sws_context_ = sws_getContext(video_decoder_ctx_->width, video_decoder_ctx_->height, video_decoder_ctx_->pix_fmt,
                                      video_decoder_ctx_->width, video_decoder_ctx_->height, AV_PIX_FMT_RGB0,
                                      SWS_BICUBLIN | SWS_BITEXACT, nullptr, nullptr, nullptr);
        if (!sws_context_)
        {
            qWarning("Failed to open swscale context");
            return;
        }
    }

    /* find the audio stream information */
    AVCodec *audio_decoder = nullptr;
    ret = av_find_best_stream(input_ctx_.Get(), AVMEDIA_TYPE_AUDIO, -1, -1, &audio_decoder, 0);
    if (ret < 0)
    {
        qWarning("Cannot find a audio stream in the input file");
        return;
    }
    audio_stream_index_ = ret;

    AVStream *audio_stream = input_ctx_->streams[audio_stream_index_];

    if (!(audio_decoder_ctx_ = avcodec_alloc_context3(audio_decoder)))
    {
        qWarning("Failed to alloc codec for audio stream #%u", ret);
        return;
    }

    if (avcodec_parameters_to_context(audio_decoder_ctx_.Get(), audio_stream->codecpar) < 0)
        return;

    if ((ret = avcodec_open2(audio_decoder_ctx_.Get(), audio_decoder, NULL)) < 0)
    {
        qWarning("Failed to open codec for audio stream #%u", ret);
        return;
    }

    video_stream_time_base_ = input_ctx_->streams[video_stream_index_]->time_base;
    audio_stream_time_base_ = input_ctx_->streams[audio_stream_index_]->time_base;
}

void LiveStreamSource::start()
{
    emit newMedia();
    Synchronize();
    StartPushTick();
    debugSourceTick();
}

void LiveStreamSource::onPushTick()
{
    if (!playing() && IsBufferLongerThan(kFrameBufferStartThreshold))
    {
        StartPlaying();
    }
    if (playing() && !video_frames_.empty() && !audio_frames_.empty())
    {
        pushed_time_ += kFrameBufferPushInterval;

        auto video_itr = video_frames_.begin(), video_itr_end = video_frames_.end();
        for (; video_itr != video_itr_end; ++video_itr)
        {
            VideoFrame &frame = **video_itr;
            auto duration = AVTimestampToDuration<std::chrono::microseconds>(frame.timestamp, video_stream_time_base_);
            if (duration >= pushed_time_)
                break;
            frame.present_time = base_time_ + duration + kUploadToRenderLatency;
            emit newVideoFrame(*video_itr);
        }
        video_frames_.erase(video_frames_.begin(), video_itr);

        auto audio_itr = audio_frames_.begin(), audio_itr_end = audio_frames_.end();
        for (; audio_itr != audio_itr_end; ++audio_itr)
        {
            AudioFrame &frame = **audio_itr;
            auto duration = AVTimestampToDuration<std::chrono::microseconds>(frame.timestamp, audio_stream_time_base_);
            if (duration >= pushed_time_)
                break;
            frame.present_time = base_time_ + duration + kUploadToRenderLatency;
            emit newAudioFrame(*audio_itr);
        }
        audio_frames_.erase(audio_frames_.begin(), audio_itr);

        if (video_frames_.empty() || audio_frames_.empty())
        {
            qDebug("Frame buffer is empty");
            StopPlaying();
        }
    }
    SetUpNextPushTick();
}

void LiveStreamSource::debugSourceTick()
{
    int ret = 0;
    AVPacket packet;
    while (!IsBufferLongerThan(kFrameBufferFullThreshold))
    {
        if ((ret = av_read_frame(input_ctx_.Get(), &packet)) < 0)
            return;

        if (packet.stream_index == video_stream_index_)
        {
            ret = avcodec_send_packet(video_decoder_ctx_.Get(), &packet);
            av_packet_unref(&packet);
            if (ret == AVERROR_EOF)
            {
                return;
            }
            else if (ret < 0)
            {
                qWarning("Error while decoding video (sending packet) #%u", ret);
                return;
            }
            do
            {
                ret = ReceiveVideoFrame();
            } while (ret >= 0);
        }
        else if (packet.stream_index == audio_stream_index_)
        {
            ret = avcodec_send_packet(audio_decoder_ctx_.Get(), &packet);
            av_packet_unref(&packet);
            if (ret == AVERROR_EOF)
            {
                return;
            }
            else if (ret < 0)
            {
                qWarning("Error while decoding video (sending packet) #%u", ret);
                return;
            }
            do
            {
                ret = ReceiveAudioFrame();
            } while (ret >= 0);
        }
        else
        {
            av_packet_unref(&packet);
        }
    }
    QTimer::singleShot(20, this, &LiveStreamSource::debugSourceTick);
}

bool LiveStreamSource::IsBufferLongerThan(PlaybackClock::duration duration)
{
    if (video_frames_.empty() || audio_frames_.empty())
        return false;
    if (AVTimestampToDuration<std::chrono::microseconds>(video_frames_.back()->timestamp - video_frames_.front()->timestamp, video_stream_time_base_) < duration)
        return false;
    if (AVTimestampToDuration<std::chrono::microseconds>(audio_frames_.back()->timestamp - audio_frames_.front()->timestamp, audio_stream_time_base_) < duration)
        return false;
    return true;
}

void LiveStreamSource::StartPlaying()
{
    Q_ASSERT(!video_frames_.empty() && !audio_frames_.empty());
    playing_ = true;
    auto current_time = PlaybackClock::now();
    base_time_ = std::min(current_time - AVTimestampToDuration<std::chrono::microseconds>(video_frames_.front()->timestamp, video_stream_time_base_),
                          current_time - AVTimestampToDuration<std::chrono::microseconds>(audio_frames_.front()->timestamp, audio_stream_time_base_));
    emit playingChanged();
}

void LiveStreamSource::StopPlaying()
{
    playing_ = false;
    emit playingChanged();
}

void LiveStreamSource::Synchronize()
{
    pushed_time_ = std::chrono::milliseconds(0);
}

int LiveStreamSource::ReceiveVideoFrame()
{
    AVFrameObject frame;
    int ret = 0;

    if (!(frame = av_frame_alloc()))
    {
        qWarning("Can not alloc frame");
        return AVERROR(ENOMEM);
    }

    ret = avcodec_receive_frame(video_decoder_ctx_.Get(), frame.Get());
    if (ret == AVERROR(EAGAIN) || ret == AVERROR_EOF)
    {
        return ret;
    }
    else if (ret < 0)
    {
        qWarning("Error while decoding video (receiving frame) #%u", ret);
        return ret;
    }

    if (frame->format == hw_pix_fmt) {
        D3D11_TEXTURE2D_DESC texture_desc;

        ID3D11Texture2D *decoded_texture = reinterpret_cast<ID3D11Texture2D *>(frame->data[0]);
        const int decoded_texture_index = reinterpret_cast<uintptr_t>(frame->data[1]);
        decoded_texture->GetDesc(&texture_desc);
        int width = texture_desc.Width;
        int height = texture_desc.Height;
        Q_ASSERT(width > 0 || height > 0);

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
            qWarning("Error while creating nv12_texture");
            return AVERROR(ENOMEM);
        }

        AVD3D11VADeviceContextUniqueLock lock(shared_resource->d3d11_device_ctx);
        shared_resource->device_context->CopySubresourceRegion(nv12_texture.Get(), 0, 0, 0, 0, decoded_texture, decoded_texture_index, nullptr);
        lock.Unlock();

        QSharedPointer<VideoFrame> video_frame = QSharedPointer<VideoFrame>::create();
        video_frame->timestamp = frame->pts;
        video_frame->size = QSize(width, height);
        video_frame->texture = std::move(nv12_texture);
        video_frame->is_rgbx = false;

        Q_ASSERT(video_frame->texture);
        video_frames_.push_back(std::move(video_frame));
    }
    else
    {
        Q_ASSERT(frame->format == video_decoder_ctx_->pix_fmt);
        Q_ASSERT(frame->width == video_decoder_ctx_->width);
        Q_ASSERT(frame->height == video_decoder_ctx_->height);

        int width = frame->width, height = frame->height;
        int dst_line_size = sizeof(uint32_t) * width;
        int dst_size = dst_line_size * height;
        std::unique_ptr<uint8_t[]> dst_buffer = std::make_unique<uint8_t[]>(dst_size);
        uint8_t *dst_data[3] = { dst_buffer.get(), nullptr, nullptr };
        int dst_stride[3] = { dst_line_size, 0, 0 };
        ret = sws_scale(sws_context_.Get(), frame->data, frame->linesize, 0, height, dst_data, dst_stride);
        if (ret < 0)
        {
            qWarning("Error while color format converting");
            return ret;
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
            qWarning("Error while creating rgb_texture");
            return AVERROR(ENOMEM);
        }

        QSharedPointer<VideoFrame> video_frame = QSharedPointer<VideoFrame>::create();
        video_frame->timestamp = frame->pts;
        video_frame->size = QSize(width, height);
        video_frame->texture = std::move(rgbx_texture);
        video_frame->is_rgbx = true;

        Q_ASSERT(video_frame->texture);
        video_frames_.push_back(std::move(video_frame));
    }

    return 0;
}

int LiveStreamSource::ReceiveAudioFrame()
{
    AVFrameObject frame;
    int ret = 0;

    if (!(frame = av_frame_alloc()))
    {
        qWarning("Can not alloc frame");
        return AVERROR(ENOMEM);
    }

    ret = avcodec_receive_frame(audio_decoder_ctx_.Get(), frame.Get());
    if (ret == AVERROR(EAGAIN) || ret == AVERROR_EOF)
    {
        return ret;
    }
    else if (ret < 0)
    {
        qWarning("Error while decoding audio #%u", ret);
        return ret;
    }

    QSharedPointer<AudioFrame> audio_frame = QSharedPointer<AudioFrame>::create();
    audio_frame->timestamp = frame->pts;
    audio_frame->frame = std::move(frame);
    audio_frames_.push_back(std::move(audio_frame));

    return 0;
}

void LiveStreamSource::StartPushTick()
{
    push_tick_time_ = PlaybackClock::now();
    push_tick_enabled_ = true;
    SetUpNextPushTick();
}

void LiveStreamSource::StopPushTick()
{
    push_tick_enabled_ = false;
}

void LiveStreamSource::SetUpNextPushTick()
{
    if (!push_tick_enabled_)
        return;
    push_tick_time_ += kFrameBufferPushInterval;
    auto next_sleep_time = std::chrono::duration_cast<std::chrono::milliseconds>(push_tick_time_ - PlaybackClock::now());
    if (next_sleep_time.count() <= 0)
    {
        emit queuePushTick();
    }
    else
    {
        QTimer::singleShot(next_sleep_time, this, &LiveStreamSource::onPushTick);
    }
}
