#include "pch.h"
#include "LiveStreamSource.h"

#include "D3D11SharedResource.h"

Q_LOGGING_CATEGORY(CategoryStreamDecoding, "qddm.decode")

static constexpr int kInputBufferSize = 0x1000;

static constexpr AVHWDeviceType kHwDeviceType = AV_HWDEVICE_TYPE_D3D11VA;
static constexpr AVPixelFormat kHwPixelFormat = AV_PIX_FMT_D3D11;

static constexpr PlaybackClock::duration kPacketBufferStartThreshold = std::chrono::milliseconds(5000), kPacketBufferFullThreshold = std::chrono::milliseconds(5000);
static constexpr PlaybackClock::duration kFrameBufferStartThreshold = std::chrono::milliseconds(200), kFrameBufferFullThreshold = std::chrono::milliseconds(200);
static constexpr std::chrono::milliseconds kFrameBufferPushInterval = std::chrono::milliseconds(50);
static constexpr PlaybackClock::duration kUploadToRenderLatency = std::chrono::milliseconds(200);

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
        if (*p == kHwPixelFormat)
            return *p;
    }

    //Return preference of decoder otherwise
    return *pix_fmts;
}

LiveStreamSource::LiveStreamSource(QObject *parent)
    :QObject(parent), demuxer_io_lock_(&demuxer_io_mutex_)
{
    demuxer_io_lock_.unlock();

    push_timer_ = new QTimer(this);
    push_timer_->setSingleShot(true);

    connect(push_timer_, &QTimer::timeout, this, &LiveStreamSource::OnPushTick);
}

LiveStreamSource::~LiveStreamSource()
{
    demuxer_input_ = nullptr;
    demuxer_io_lock_.unlock();
    demuxer_io_condition_.notify_all();
    demuxer_thread_.quit();
    demuxer_thread_.wait();

    push_timer_->stop();
}

void LiveStreamSource::OnNewInputStream(const char *url_hint, QIODevice *source, int64_t probe_size)
{
    if (open())
        Close();

    int i, ret;

    demuxer_input_ = source;

    AVAllocatedMemory input_buffer_ = (uint8_t *)av_malloc(kInputBufferSize);
    if (!(input_ctx_ = avio_alloc_context(input_buffer_.Get(), kInputBufferSize, 0, this, AVIOReadCallback, nullptr, nullptr)))
    {
        qCWarning(CategoryStreamDecoding, "Failed to alloc avio context");
        Close();
        return;
    }
    input_buffer_.DetachObject();

    if (!(demuxer_ctx_ = avformat_alloc_context()))
    {
        qCWarning(CategoryStreamDecoding, "Failed to allocate demuxer context");
        Close();
        return;
    }
    demuxer_ctx_->pb = input_ctx_.Get();
    if (avformat_open_input(demuxer_ctx_.GetAddressOf(), url_hint, NULL, NULL) != 0)
    {
        qCWarning(CategoryStreamDecoding, "Cannot open input");
        Close();
        return;
    }

    if (probe_size > 0)
        demuxer_ctx_->probesize = probe_size;
    if (avformat_find_stream_info(demuxer_ctx_.Get(), NULL) < 0)
    {
        qCWarning(CategoryStreamDecoding, "Cannot find input stream information.");
        Close();
        return;
    }

    /* find the video stream information */
    AVCodec *video_decoder = nullptr;
    ret = av_find_best_stream(demuxer_ctx_.Get(), AVMEDIA_TYPE_VIDEO, -1, -1, &video_decoder, 0);
    if (ret < 0)
    {
        qCWarning(CategoryStreamDecoding, "Cannot find a video stream in the input file");
        Close();
        return;
    }
    video_stream_index_ = ret;

    AVStream *video_stream = demuxer_ctx_->streams[video_stream_index_];

    for (i = 0;; i++)
    {
        const AVCodecHWConfig *config = avcodec_get_hw_config(video_decoder, i);
        if (!config)
        {
            qCWarning(CategoryStreamDecoding, "Decoder %s does not support device type %s.", video_decoder->name, av_hwdevice_get_type_name(kHwDeviceType));
            Close();
            return;
        }
        if (config->methods & AV_CODEC_HW_CONFIG_METHOD_HW_DEVICE_CTX && config->device_type == kHwDeviceType && kHwPixelFormat == config->pix_fmt)
        {
            break;
        }
    }

    if (!(video_decoder_ctx_ = avcodec_alloc_context3(video_decoder)))
    {
        qCWarning(CategoryStreamDecoding, "Failed to alloc codec for video stream #%u", ret);
        Close();
        return;
    }

    if (avcodec_parameters_to_context(video_decoder_ctx_.Get(), video_stream->codecpar) < 0)
    {
        Close();
        return;
    }

    video_decoder_ctx_->hw_device_ctx = av_buffer_ref(D3D11SharedResource::resource->hw_device_ctx_obj.Get());

    if ((ret = avcodec_open2(video_decoder_ctx_.Get(), video_decoder, NULL)) < 0)
    {
        qCWarning(CategoryStreamDecoding, "Failed to open codec for video stream #%u", ret);
        Close();
        return;
    }

    if (video_decoder_ctx_->pix_fmt != kHwPixelFormat)
    {
        sws_context_ = sws_getContext(video_decoder_ctx_->width, video_decoder_ctx_->height, video_decoder_ctx_->pix_fmt,
                                      video_decoder_ctx_->width, video_decoder_ctx_->height, AV_PIX_FMT_RGB0,
                                      SWS_POINT | SWS_BITEXACT, nullptr, nullptr, nullptr);
        if (!sws_context_)
        {
            qCWarning(CategoryStreamDecoding, "Failed to open swscale context");
            Close();
            return;
        }
    }

    /* find the audio stream information */
    AVCodec *audio_decoder = nullptr;
    ret = av_find_best_stream(demuxer_ctx_.Get(), AVMEDIA_TYPE_AUDIO, -1, -1, &audio_decoder, 0);
    if (ret < 0)
    {
        qCWarning(CategoryStreamDecoding, "Cannot find a audio stream in the input file");
        Close();
        return;
    }
    audio_stream_index_ = ret;

    AVStream *audio_stream = demuxer_ctx_->streams[audio_stream_index_];

    if (!(audio_decoder_ctx_ = avcodec_alloc_context3(audio_decoder)))
    {
        qCWarning(CategoryStreamDecoding, "Failed to alloc codec for audio stream #%u", ret);
        Close();
        return;
    }

    if (avcodec_parameters_to_context(audio_decoder_ctx_.Get(), audio_stream->codecpar) < 0)
    {
        Close();
        return;
    }

    if ((ret = avcodec_open2(audio_decoder_ctx_.Get(), audio_decoder, NULL)) < 0)
    {
        qCWarning(CategoryStreamDecoding, "Failed to open codec for audio stream #%u", ret);
        Close();
        return;
    }

    video_stream_time_base_ = demuxer_ctx_->streams[video_stream_index_]->time_base;
    audio_stream_time_base_ = demuxer_ctx_->streams[audio_stream_index_]->time_base;

    open_ = true;
    demuxer_io_lock_.relock();

    LiveStreamSourceDemuxWorker *worker = new LiveStreamSourceDemuxWorker(this);
    worker->moveToThread(&demuxer_thread_);
    connect(&demuxer_thread_, &QThread::finished, worker, &QObject::deleteLater);
    demuxer_thread_.start();
    QMetaObject::invokeMethod(worker, "Work");

    emit newMedia(video_decoder_ctx_.Get(), audio_decoder_ctx_.Get());
    InitPlaying();
    StartPushTick();
}

void LiveStreamSource::OnNewInputData()
{
    if (Q_UNLIKELY(!open()))
        return;
    NotifyAndWaitDemuxer();
}

void LiveStreamSource::OnDeleteInputStream()
{
    Close();
}

int LiveStreamSource::AVIOReadCallback(void *opaque, uint8_t *buf, int buf_size)
{
    LiveStreamSource *self = static_cast<LiveStreamSource *>(opaque);

    if (Q_UNLIKELY(!self->open()))
    {
        QMutexLocker lock(&self->demuxer_io_mutex_);
        if (Q_UNLIKELY(self->demuxer_input_ == nullptr))
            return AVERROR_EOF;
        qint64 read_size = self->demuxer_input_->read(reinterpret_cast<char *>(buf), buf_size);
        if (read_size == 0)
            return AVERROR(EAGAIN);
        if (read_size < 0)
            return AVERROR_EOF;
        return (int)read_size;
    }

    qint64 read_size;
    QMutexLocker lock(&self->demuxer_io_mutex_);
    while (self->demuxer_input_ != nullptr && (read_size = self->demuxer_input_->read(reinterpret_cast<char *>(buf), buf_size)) == 0)
        self->demuxer_io_condition_.wait(lock.mutex());

    if (Q_UNLIKELY(self->demuxer_input_ == nullptr))
    {
        lock.unlock();
        self->demuxer_io_condition_.notify_all();
        return AVERROR_EOF;
    }
    if (Q_UNLIKELY(read_size <= 0))
    {
        self->demuxer_input_->close();
        self->demuxer_input_ = nullptr;
        lock.unlock();
        self->demuxer_io_condition_.notify_all();
        return AVERROR_EOF;
    }
    if (self->demuxer_input_->bytesAvailable() <= 0)
    {
        lock.unlock();
        self->demuxer_io_condition_.notify_all();
    }
    return (int)read_size;
}

void LiveStreamSourceDemuxWorker::Work()
{
    int ret;
    LiveStreamSource::AVPacketObject packet;

    while (true)
    {
        ret = av_read_frame(parent_->demuxer_ctx_.Get(), packet.ReleaseAndGet());
        if (Q_UNLIKELY(ret < 0))
        {
            if (ret != AVERROR_EOF)
            {
                qCWarning(CategoryStreamDecoding, "Error while decoding video (reading frame) #%u", ret);
            }
            QMutexLocker lock(&parent_->demuxer_io_mutex_);
            parent_->demuxer_input_ = nullptr;
            lock.unlock();
            parent_->demuxer_io_condition_.notify_all();
            return;
        }
        packet.SetOwn();

        if (packet->stream_index == parent_->video_stream_index_)
        {
            QMutexLocker lock(&parent_->demuxer_io_mutex_);
            while (parent_->demuxer_input_ != nullptr && parent_->IsPacketBufferLongerThan(kPacketBufferFullThreshold))
                parent_->demuxer_io_condition_.wait(lock.mutex());
            if (Q_UNLIKELY(parent_->demuxer_input_ == nullptr))
                return;
            parent_->video_packets_.push_back(std::move(packet));
            if (parent_->IsPacketBufferLongerThan(kPacketBufferFullThreshold))
            {
                lock.unlock();
                parent_->demuxer_io_condition_.notify_all();
            }
        }
        else if (packet->stream_index == parent_->audio_stream_index_)
        {
            QMutexLocker lock(&parent_->demuxer_io_mutex_);
            while (parent_->demuxer_input_ != nullptr && parent_->IsPacketBufferLongerThan(kPacketBufferFullThreshold))
                parent_->demuxer_io_condition_.wait(lock.mutex());
            if (Q_UNLIKELY(parent_->demuxer_input_ == nullptr))
                return;
            parent_->audio_packets_.push_back(std::move(packet));
            if (parent_->IsPacketBufferLongerThan(kPacketBufferFullThreshold))
            {
                lock.unlock();
                parent_->demuxer_io_condition_.notify_all();
            }
        }
    }
}

void LiveStreamSource::NotifyAndWaitDemuxer()
{
    demuxer_io_lock_.unlock();
    demuxer_io_condition_.notify_all();
    demuxer_io_lock_.relock();
    while (demuxer_input_ != nullptr && demuxer_input_->bytesAvailable() > 0 && !IsPacketBufferLongerThan(kPacketBufferFullThreshold))
        demuxer_io_condition_.wait(demuxer_io_lock_.mutex());
}

void LiveStreamSource::Decode()
{
    if (Q_UNLIKELY(!open()))
        return;

    int ret;

    int64_t timestamp_video_front, timestamp_audio_front;
    int64_t timestamp_video_frame_full, timestamp_audio_frame_full;

    if (!video_frames_.empty())
    {
        timestamp_video_front = video_frames_.front()->timestamp;
        timestamp_video_frame_full = timestamp_video_front + DurationToAVTimestamp(kFrameBufferFullThreshold, video_stream_time_base_);
    }
    else if (!video_packets_.empty())
    {
        timestamp_video_front = video_packets_.front()->pts;
        timestamp_video_frame_full = timestamp_video_front + DurationToAVTimestamp(kFrameBufferFullThreshold, video_stream_time_base_);
    }
    else
    {
        timestamp_video_front = -1;
    }
    if (!audio_frames_.empty())
    {
        timestamp_audio_front = audio_frames_.front()->timestamp;
        timestamp_audio_frame_full = timestamp_audio_front + DurationToAVTimestamp(kFrameBufferFullThreshold, audio_stream_time_base_);
    }
    else if (!audio_packets_.empty())
    {
        timestamp_audio_front = audio_packets_.front()->pts;
        timestamp_audio_frame_full = timestamp_audio_front + DurationToAVTimestamp(kFrameBufferFullThreshold, audio_stream_time_base_);
    }
    else
    {
        timestamp_audio_front = -1;
    }

    if (!video_packets_.empty())
    {
        while (!video_packets_.empty() && (video_frames_.empty() || video_frames_.back()->timestamp < timestamp_video_frame_full))
        {
            ret = SendVideoPacket(*video_packets_.front());
            video_packets_.erase(video_packets_.begin());
            if (ret == AVERROR_EOF)
            {
                break;
            }
            else if (ret < 0)
            {
                Close();
                return;
            }
        }
    }
    if (!audio_packets_.empty())
    {
        while (!audio_packets_.empty() && (audio_frames_.empty() || audio_frames_.back()->timestamp < timestamp_audio_frame_full))
        {
            ret = SendAudioPacket(*audio_packets_.front());
            audio_packets_.erase(audio_packets_.begin());
            if (ret == AVERROR_EOF)
            {
                break;
            }
            else if (ret < 0)
            {
                Close();
                return;
            }
        }
    }

    NotifyAndWaitDemuxer();
}

int LiveStreamSource::SendVideoPacket(AVPacket &packet)
{
    if (video_eof_met_)
        return AVERROR_EOF;
    int ret = 0;
    ret = avcodec_send_packet(video_decoder_ctx_.Get(), &packet);
    if (ret < 0)
    {
        if (ret == AVERROR_EOF)
            video_eof_met_ = true;
        else
            qCWarning(CategoryStreamDecoding, "Error while decoding video (sending packet) #%u", ret);
        return ret;
    }
    do
    {
        ret = ReceiveVideoFrame();
    } while (ret >= 0);
    return ret == AVERROR(EAGAIN) ? 0 : ret;
}

int LiveStreamSource::SendAudioPacket(AVPacket &packet)
{
    if (audio_eof_met_)
        return AVERROR_EOF;
    int ret = 0;
    ret = avcodec_send_packet(audio_decoder_ctx_.Get(), &packet);
    if (ret < 0)
    {
        if (ret == AVERROR_EOF)
            audio_eof_met_ = true;
        else
            qCWarning(CategoryStreamDecoding, "Error while decoding audio (sending packet) #%u", ret);
        return ret;
    }
    do
    {
        ret = ReceiveAudioFrame();
    } while (ret >= 0);
    return ret == AVERROR(EAGAIN) ? 0 : ret;
}

int LiveStreamSource::ReceiveVideoFrame()
{
    AVFrameObject frame;
    int ret = 0;

    if (!(frame = av_frame_alloc()))
    {
        qCWarning(CategoryStreamDecoding, "Can not alloc frame");
        return AVERROR(ENOMEM);
    }

    ret = avcodec_receive_frame(video_decoder_ctx_.Get(), frame.Get());
    if (ret == AVERROR(EAGAIN))
    {
        return ret;
    }
    else if (ret == AVERROR_EOF)
    {
        video_eof_met_ = true;
        return ret;
    }
    else if (ret < 0)
    {
        qCWarning(CategoryStreamDecoding, "Error while decoding video (receiving frame) #%u", ret);
        return ret;
    }

    if (frame->format == kHwPixelFormat)
    {
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
            qCWarning(CategoryStreamDecoding, "Error while creating nv12_texture");
            return AVERROR(ENOMEM);
        }

        AVD3D11VADeviceContextUniqueLock lock(shared_resource->d3d11_device_ctx);
        shared_resource->device_context->CopySubresourceRegion(nv12_texture.Get(), 0, 0, 0, 0, decoded_texture, decoded_texture_index, nullptr);
        lock.Unlock();

        QSharedPointer<VideoFrame> video_frame = QSharedPointer<VideoFrame>::create();
        video_frame->timestamp = frame->pts;
        video_frame->size = QSize(width, height);
        video_frame->texture = std::move(nv12_texture);
        video_frame->texture_format = VideoFrame::NV12;

        Q_ASSERT(video_frame->texture);
        video_frames_.push_back(std::move(video_frame));
    }
    else if (frame->format == AV_PIX_FMT_YUVJ444P)
    {
        Q_ASSERT(frame->format == video_decoder_ctx_->pix_fmt);
        Q_ASSERT(frame->width == video_decoder_ctx_->width);
        Q_ASSERT(frame->height == video_decoder_ctx_->height);

        int width = frame->width, height = frame->height;

        D3D11_TEXTURE2D_DESC texture_desc;
        ZeroMemory(&texture_desc, sizeof(texture_desc));
        texture_desc.Format = DXGI_FORMAT_R8_UNORM;          // Pixel format
        texture_desc.Width = width;                          // Width of the video frames
        texture_desc.Height = height;                        // Height of the video frames
        texture_desc.ArraySize = 3;                          // Number of textures in the array
        texture_desc.MipLevels = 1;                          // Number of miplevels in each texture
        texture_desc.BindFlags = D3D11_BIND_SHADER_RESOURCE; // We read from this texture in the shader
        texture_desc.Usage = D3D11_USAGE_IMMUTABLE;
        texture_desc.MiscFlags = 0;
        texture_desc.CPUAccessFlags = 0;
        texture_desc.SampleDesc.Count = 1;
        D3D11_SUBRESOURCE_DATA texture_init_data[3];
        ZeroMemory(&texture_init_data, sizeof(texture_init_data));
        texture_init_data[0].pSysMem = frame->data[0];
        texture_init_data[0].SysMemPitch = frame->linesize[0];
        texture_init_data[1].pSysMem = frame->data[1];
        texture_init_data[1].SysMemPitch = frame->linesize[1];
        texture_init_data[2].pSysMem = frame->data[2];
        texture_init_data[2].SysMemPitch = frame->linesize[2];

        ComPtr<ID3D11Texture2D> yuvj444p_texture = nullptr;
        D3D11SharedResource *shared_resource = D3D11SharedResource::resource;
        HRESULT hr;
        hr = shared_resource->device->CreateTexture2D(&texture_desc, texture_init_data, &yuvj444p_texture);
        if (FAILED(hr))
        {
            qCWarning(CategoryStreamDecoding, "Error while creating rgb_texture");
            return AVERROR(ENOMEM);
        }

        QSharedPointer<VideoFrame> video_frame = QSharedPointer<VideoFrame>::create();
        video_frame->timestamp = frame->pts;
        video_frame->size = QSize(width, height);
        video_frame->texture = std::move(yuvj444p_texture);
        video_frame->texture_format = VideoFrame::YUVJ444P;

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
        thread_local std::vector<uint8_t> dst_buffer;
        dst_buffer.resize(dst_size);
        uint8_t *dst_data[3] = { dst_buffer.data(), nullptr, nullptr };
        int dst_stride[3] = { dst_line_size, 0, 0 };
        ret = sws_scale(sws_context_.Get(), frame->data, frame->linesize, 0, height, dst_data, dst_stride);
        if (ret < 0)
        {
            qCWarning(CategoryStreamDecoding, "Error while color format converting");
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
        texture_init_data.pSysMem = dst_buffer.data();
        texture_init_data.SysMemPitch = dst_line_size;

        ComPtr<ID3D11Texture2D> rgbx_texture = nullptr;
        D3D11SharedResource *shared_resource = D3D11SharedResource::resource;
        HRESULT hr;
        hr = shared_resource->device->CreateTexture2D(&texture_desc, &texture_init_data, &rgbx_texture);
        if (FAILED(hr))
        {
            qCWarning(CategoryStreamDecoding, "Error while creating rgb_texture");
            return AVERROR(ENOMEM);
        }

        QSharedPointer<VideoFrame> video_frame = QSharedPointer<VideoFrame>::create();
        video_frame->timestamp = frame->pts;
        video_frame->size = QSize(width, height);
        video_frame->texture = std::move(rgbx_texture);
        video_frame->texture_format = VideoFrame::RGBX;

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
        qCWarning(CategoryStreamDecoding, "Can not alloc frame");
        return AVERROR(ENOMEM);
    }

    ret = avcodec_receive_frame(audio_decoder_ctx_.Get(), frame.Get());
    if (ret == AVERROR(EAGAIN))
    {
        return ret;
    }
    else if (ret == AVERROR_EOF)
    {
        audio_eof_met_ = true;
        return ret;
    }
    else if (ret < 0)
    {
        qCWarning(CategoryStreamDecoding, "Error while decoding audio #%u", ret);
        return ret;
    }

    QSharedPointer<AudioFrame> audio_frame = QSharedPointer<AudioFrame>::create();
    audio_frame->timestamp = frame->pts;
    audio_frame->frame = std::move(frame);
    audio_frame->sample_format = audio_decoder_ctx_->sample_fmt;
    audio_frames_.push_back(std::move(audio_frame));

    return 0;
}

void LiveStreamSource::StartPushTick()
{
    push_tick_time_ = PlaybackClock::now();
    push_tick_enabled_ = true;
#ifdef _DEBUG
    last_debug_report_ = PlaybackClock::now();
#endif
    SetUpNextPushTick();
}

void LiveStreamSource::StopPushTick()
{
    push_tick_enabled_ = false;
    push_timer_->stop();
}

void LiveStreamSource::OnPushTick()
{
    if (!playing() && IsFrameBufferLongerThan(kFrameBufferStartThreshold) && IsPacketBufferLongerThan(kPacketBufferStartThreshold))
    {
        StartPlaying();
    }
    if (playing())
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

        if (Q_LIKELY(demuxer_input_ != nullptr))
        {
            if (video_frames_.empty() || audio_frames_.empty())
            {
                qCDebug(CategoryStreamDecoding, "Frame buffer is empty");
                StopPlaying();
            }
        }
        else
        {
            if (video_frames_.empty() && audio_frames_.empty())
            {
                qCDebug(CategoryStreamDecoding, "Frame buffer is empty and end of file reached");
                Close();
                return;
            }
        }
    }

#ifdef _DEBUG
    if (last_debug_report_ - PlaybackClock::now() > std::chrono::seconds(1))
    {
        last_debug_report_ += std::chrono::seconds(1);
        qCDebug(CategoryStreamDecoding, "Video packet buffer: %lldms", video_packets_.empty() ? 0ll : AVTimestampToDuration<std::chrono::milliseconds>(video_packets_.back()->pts - video_packets_.front()->pts, video_stream_time_base_).count());
        qCDebug(CategoryStreamDecoding, "Audio packet buffer: %lldms", audio_packets_.empty() ? 0ll : AVTimestampToDuration<std::chrono::milliseconds>(audio_packets_.back()->pts - audio_packets_.front()->pts, audio_stream_time_base_).count());
        qCDebug(CategoryStreamDecoding, "Video frame buffer: %lldms", video_frames_.empty() ? 0ll : AVTimestampToDuration<std::chrono::milliseconds>(video_frames_.back()->timestamp - video_frames_.front()->timestamp, video_stream_time_base_).count());
        qCDebug(CategoryStreamDecoding, "Audio frame buffer: %lldms", audio_frames_.empty() ? 0ll : AVTimestampToDuration<std::chrono::milliseconds>(audio_frames_.back()->timestamp - audio_frames_.front()->timestamp, audio_stream_time_base_).count());
    }
#endif

    Decode(); //Try to pull some data
    SetUpNextPushTick();
}

void LiveStreamSource::SetUpNextPushTick()
{
    if (!push_tick_enabled_)
        return;
    push_tick_time_ += kFrameBufferPushInterval;
    auto next_sleep_time = std::chrono::duration_cast<std::chrono::milliseconds>(push_tick_time_ - PlaybackClock::now());
    if (next_sleep_time.count() <= 0)
    {
        push_timer_->start(0);
    }
    else
    {
        push_timer_->start(next_sleep_time);
    }
}

bool LiveStreamSource::IsPacketBufferLongerThan(PlaybackClock::duration duration)
{
    if (video_packets_.empty() || audio_packets_.empty())
        return false;
    if (AVTimestampToDuration<std::chrono::microseconds>(video_packets_.back()->pts - video_packets_.front()->pts, video_stream_time_base_) < duration)
        return false;
    if (AVTimestampToDuration<std::chrono::microseconds>(audio_packets_.back()->pts - audio_packets_.front()->pts, audio_stream_time_base_) < duration)
        return false;
    return true;
}

bool LiveStreamSource::IsFrameBufferLongerThan(PlaybackClock::duration duration)
{
    if (video_frames_.empty() || audio_frames_.empty())
        return false;
    if (AVTimestampToDuration<std::chrono::microseconds>(video_frames_.back()->timestamp - video_frames_.front()->timestamp, video_stream_time_base_) < duration)
        return false;
    if (AVTimestampToDuration<std::chrono::microseconds>(audio_frames_.back()->timestamp - audio_frames_.front()->timestamp, audio_stream_time_base_) < duration)
        return false;
    return true;
}

void LiveStreamSource::InitPlaying()
{
    video_eof_met_ = audio_eof_met_ = false;
}

void LiveStreamSource::StartPlaying()
{
    Q_ASSERT(!video_frames_.empty() && !audio_frames_.empty());
    playing_ = true;
    auto current_time = PlaybackClock::now();
    auto video_played_duration = AVTimestampToDuration<std::chrono::microseconds>(video_frames_.front()->timestamp, video_stream_time_base_);
    auto audio_played_duration = AVTimestampToDuration<std::chrono::microseconds>(audio_frames_.front()->timestamp, audio_stream_time_base_);
    auto played_duration = std::max(video_played_duration, audio_played_duration);
    pushed_time_ = std::chrono::duration_cast<std::chrono::milliseconds>(played_duration);
    base_time_ = current_time - played_duration;
    emit playingChanged();
}

void LiveStreamSource::StopPlaying()
{
    playing_ = false;
    emit playingChanged();
}

void LiveStreamSource::Close()
{
    demuxer_input_ = nullptr;
    demuxer_io_lock_.unlock();
    demuxer_io_condition_.notify_all();
    demuxer_thread_.quit();
    demuxer_thread_.wait();

    StopPushTick();
    StopPlaying();
    video_eof_met_ = audio_eof_met_ = false;
    open_ = false;
    video_frames_.clear();
    audio_frames_.clear();
    sws_context_ = nullptr;
    video_decoder_ctx_ = nullptr;
    audio_decoder_ctx_ = nullptr;
    video_packets_.clear();
    audio_packets_.clear();
    demuxer_ctx_ = nullptr;
    input_ctx_ = nullptr;

    emit deleteMedia();
}
