#include "pch.h"
#include "LiveStreamDecoder.h"

Q_LOGGING_CATEGORY(CategoryStreamDecoding, "qddm.decode")

static constexpr int kInputBufferSize = 0x1000, kInputBufferSizeLimit = 0x100000;

static constexpr PlaybackClock::duration kPacketBufferStartThreshold = 2500ms, kPacketBufferFullThreshold = 5000ms;
static constexpr PlaybackClock::duration kFrameBufferStartThreshold = 200ms, kFrameBufferFullThreshold = 200ms;
static constexpr std::chrono::milliseconds kFrameBufferPushInit = 50ms, kFrameBufferPushInterval = 50ms;
static constexpr PlaybackClock::duration kUploadToRenderLatency = 150ms;

template <typename ToDuration>
static inline constexpr ToDuration AVTimestampToDuration(int64_t timestamp, AVRational time_base)
{
    return ToDuration((timestamp * time_base.num * ToDuration::period::den) / (time_base.den * ToDuration::period::num));
}

template <typename Rep, typename Period>
static inline constexpr int64_t DurationToAVTimestamp(std::chrono::duration<Rep, Period> duration, AVRational time_base)
{
    return (duration.count() * Period::num * time_base.den) / (Period::den * time_base.num);
}

LiveStreamDecoder::LiveStreamDecoder(QObject *parent)
    :QObject(parent)
{
    push_timer_ = new QTimer(this);
    push_timer_->setSingleShot(true);

    connect(push_timer_, &QTimer::timeout, this, &LiveStreamDecoder::OnPushTick);
}

LiveStreamDecoder::~LiveStreamDecoder()
{
    {
        QMutexLocker lock(&demuxer_out_mutex_);
        demuxer_eof_ = true;
        video_packets_.clear();
        audio_packets_.clear();
    }
    demuxer_out_condition_.notify_all();
    demuxer_in_.Close();
    demuxer_thread_.quit();
    demuxer_thread_.wait();

    push_timer_->stop();
}

void LiveStreamDecoder::BeginData()
{
    demuxer_in_.Open();
}

size_t LiveStreamDecoder::PushData(const char *data, size_t size)
{
    return demuxer_in_.Write(reinterpret_cast<const uint8_t *>(data), size);
}

void LiveStreamDecoder::PushData(QIODevice *device)
{
    demuxer_in_.Fill(device, std::min<qint64>(kInputBufferSizeLimit, device->bytesAvailable()));
}

void LiveStreamDecoder::EndData()
{
    demuxer_in_.End();
}

void LiveStreamDecoder::CloseData()
{
    demuxer_in_.Close();
}

void LiveStreamDecoder::onNewInputStream(const QString &url_hint, const QString &record_path)
{
    if (open_)
        return;

    remuxer_out_path_oneshot_ = record_path;

    int i, ret;

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
    auto url_string = url_hint.toLocal8Bit();
    if (avformat_open_input(demuxer_ctx_.GetAddressOf(), url_string.data(), NULL, NULL) != 0)
    {
        qCWarning(CategoryStreamDecoding, "Cannot open input");
        Close();
        return;
    }

    demuxer_ctx_->max_analyze_duration = AV_TIME_BASE / 10;
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

    video_decoder_hw_pixel_format_ = AV_PIX_FMT_NONE;
    for (i = 0;; i++)
    {
        const AVCodecHWConfig *video_decoder_hw_config = avcodec_get_hw_config(video_decoder, i);
        if (!video_decoder_hw_config)
            break;
        if (video_decoder_hw_config->device_type != AV_HWDEVICE_TYPE_NONE && video_decoder_hw_config->pix_fmt != AV_PIX_FMT_NONE && (video_decoder_hw_config->methods & AV_CODEC_HW_CONFIG_METHOD_AD_HOC) != 0)
        {
            if ((ret = av_hwdevice_ctx_create(video_decoder_hw_ctx_.ReleaseAndGetAddressOf(), video_decoder_hw_config->device_type, NULL, NULL, 0)) >= 0)
            {
                video_decoder_hw_pixel_format_ = video_decoder_hw_config->pix_fmt;
                break;
            }
            else
            {
                qCDebug(CategoryStreamDecoding, "Failed to open hw context for video stream #%u", ret);
            }
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

    if (video_decoder_hw_pixel_format_ != AV_PIX_FMT_NONE)
    {
        video_decoder_ctx_->hw_device_ctx = av_buffer_ref(video_decoder_hw_ctx_.Get());
    }

    if ((ret = avcodec_open2(video_decoder_ctx_.Get(), video_decoder, NULL)) < 0)
    {
        qCWarning(CategoryStreamDecoding, "Failed to open codec for video stream #%u", ret);
        Close();
        return;
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

    StartRecording();

    LiveStreamSourceDemuxWorker *worker = new LiveStreamSourceDemuxWorker(this);
    worker->moveToThread(&demuxer_thread_);
    connect(&demuxer_thread_, &QThread::finished, worker, &QObject::deleteLater);
    demuxer_thread_.start();
    QMetaObject::invokeMethod(worker, "Work");

    emit newMedia(video_decoder_ctx_.Get(), audio_decoder_ctx_.Get());
    InitPlaying();
    StartPushTick();
}

void LiveStreamDecoder::onDeleteInputStream()
{
    if (open_)
        Close();
}

void LiveStreamDecoder::onSetDefaultMediaRecordFile(const QString &file_path)
{
    remuxer_out_path_default_ = file_path;
    if (remuxer_out_path_oneshot_.isEmpty())
    {
        if (remuxer_out_path_default_.isEmpty())
            StopRecording();
        else
            StartRecording();
    }
}

void LiveStreamDecoder::onSetOneshotMediaRecordFile(const QString &file_path)
{
    remuxer_out_path_oneshot_ = file_path;
    if (remuxer_out_path_default_.isEmpty())
    {
        if (remuxer_out_path_oneshot_.isEmpty())
            StopRecording();
        else
            StartRecording();
    }
}

int LiveStreamDecoder::AVIOReadCallback(void *opaque, uint8_t *buf, int buf_size)
{
    Q_ASSERT(buf_size != 0);
    LiveStreamDecoder *self = static_cast<LiveStreamDecoder *>(opaque);

    size_t read_size = self->demuxer_in_.Read(buf, buf_size);
    if (read_size == 0) //Closed
    {
        return AVERROR_EOF;
    }
    return (int)read_size;
}

void LiveStreamSourceDemuxWorker::Work()
{
    const int video_stream_index = decoder_->video_stream_index_, audio_stream_index = decoder_->audio_stream_index_;

    int ret;
    LiveStreamDecoder::AVPacketObject packet;

    while (true)
    {
        ret = av_read_frame(decoder_->demuxer_ctx_.Get(), packet.ReleaseAndGet());
        if (Q_UNLIKELY(ret < 0))
        {
            if (ret != AVERROR_EOF)
            {
                qCWarning(CategoryStreamDecoding, "Error while decoding video (reading frame) #%u", ret);
            }
            decoder_->demuxer_in_.Close();
            QMutexLocker lock(&decoder_->demuxer_out_mutex_);
            decoder_->demuxer_eof_ = true;
            return;
        }
        packet.SetOwn();
        const int packet_stream_index = packet->stream_index;

        do
        {
            QMutexLocker lock(&decoder_->remuxer_mutex_);
            if (!decoder_->remuxer_ctx_)
                break;
            const std::vector<int> &remuxer_stream_map = decoder_->remuxer_stream_map_;
            if (packet_stream_index < 0 || packet_stream_index >= (int)remuxer_stream_map.size() || remuxer_stream_map[packet_stream_index] < 0)
                break;
            LiveStreamDecoder::AVPacketObject remux_packet;
            if (av_packet_ref(remux_packet.Get(), packet.Get()) < 0)
                break;
            remux_packet.SetOwn();

            remux_packet->stream_index = remuxer_stream_map[packet_stream_index];
            AVStream *in_stream  = decoder_->demuxer_ctx_->streams[packet_stream_index];
            AVStream *out_stream = decoder_->remuxer_ctx_->streams[remux_packet->stream_index];

            remux_packet->pts = av_rescale_q_rnd(remux_packet->pts, in_stream->time_base, out_stream->time_base, static_cast<AVRounding>(AV_ROUND_NEAR_INF | AV_ROUND_PASS_MINMAX));
            remux_packet->dts = av_rescale_q_rnd(remux_packet->dts, in_stream->time_base, out_stream->time_base, static_cast<AVRounding>(AV_ROUND_NEAR_INF | AV_ROUND_PASS_MINMAX));
            remux_packet->duration = av_rescale_q(remux_packet->duration, in_stream->time_base, out_stream->time_base);
            remux_packet->pos = -1;

            ret = av_interleaved_write_frame(decoder_->remuxer_ctx_.Get(), remux_packet.Get());
            if (Q_UNLIKELY(ret < 0))
            {
                lock.unlock();
                decoder_->StopRecording();
                //TODO: see if need to restart recording
                break;
            }
        } while (false);

        if (packet_stream_index == video_stream_index)
        {
            QMutexLocker lock(&decoder_->demuxer_out_mutex_);
            while (!decoder_->demuxer_eof_ && decoder_->IsPacketBufferLongerThan(kPacketBufferFullThreshold))
                decoder_->demuxer_out_condition_.wait(lock.mutex());
            if (Q_UNLIKELY(decoder_->demuxer_eof_))
                return;
            decoder_->video_packets_.push_back(std::move(packet));
        }
        else if (packet_stream_index == audio_stream_index)
        {
            QMutexLocker lock(&decoder_->demuxer_out_mutex_);
            while (!decoder_->demuxer_eof_ && decoder_->IsPacketBufferLongerThan(kPacketBufferFullThreshold))
                decoder_->demuxer_out_condition_.wait(lock.mutex());
            if (Q_UNLIKELY(decoder_->demuxer_eof_))
                return;
            decoder_->audio_packets_.push_back(std::move(packet));
        }
    }
}

void LiveStreamDecoder::Decode()
{
    Q_ASSERT(open());

    int ret;

    int64_t timestamp_video_front, timestamp_audio_front;
    int64_t timestamp_video_frame_full, timestamp_audio_frame_full;

    QMutexLocker lock(&demuxer_out_mutex_);

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

    if (video_eof_)
    {
        video_packets_.clear();
    }
    else
    {
        auto video_packet_itr = video_packets_.begin(), video_packet_itr_end = video_packets_.end();
        while (video_packet_itr != video_packet_itr_end && (video_frames_.empty() || video_frames_.back()->timestamp < timestamp_video_frame_full))
        {
            ret = SendVideoPacket(**video_packet_itr);
            ++video_packet_itr;
            if (ret == AVERROR_EOF)
            {
                break;
            }
            else if (ret < 0)
            {
                lock.unlock();
                Close();
                return;
            }
        }
        video_packets_.erase(video_packets_.begin(), video_packet_itr);
    }

    if (audio_eof_)
    {
        audio_packets_.clear();
    }
    else
    {
        auto audio_packet_itr = audio_packets_.begin(), audio_packet_itr_end = audio_packets_.end();
        while (audio_packet_itr != audio_packet_itr_end && (audio_frames_.empty() || audio_frames_.back()->timestamp < timestamp_audio_frame_full))
        {
            ret = SendAudioPacket(**audio_packet_itr);
            ++audio_packet_itr;
            if (ret == AVERROR_EOF)
            {
                break;
            }
            else if (ret < 0)
            {
                lock.unlock();
                Close();
                return;
            }
        }
        audio_packets_.erase(audio_packets_.begin(), audio_packet_itr);
    }

    lock.unlock();
    demuxer_out_condition_.notify_all();
}

int LiveStreamDecoder::SendVideoPacket(AVPacket &packet)
{
    if (video_eof_)
        return AVERROR_EOF;
    int ret = 0;
    ret = avcodec_send_packet(video_decoder_ctx_.Get(), &packet);
    if (ret < 0)
    {
        if (ret == AVERROR_EOF)
            video_eof_ = true;
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

int LiveStreamDecoder::SendAudioPacket(AVPacket &packet)
{
    if (audio_eof_)
        return AVERROR_EOF;
    int ret = 0;
    ret = avcodec_send_packet(audio_decoder_ctx_.Get(), &packet);
    if (ret < 0)
    {
        if (ret == AVERROR_EOF)
            audio_eof_ = true;
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

int LiveStreamDecoder::ReceiveVideoFrame()
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
        video_eof_ = true;
        return ret;
    }
    else if (ret < 0)
    {
        qCWarning(CategoryStreamDecoding, "Error while decoding video (receiving frame) #%u", ret);
        return ret;
    }

    //Save important properties
    auto pts = frame->pts;

    if (frame->format == video_decoder_hw_pixel_format_) //HW pixel format, copy back first
    {
        AVFrameObject copied_frame = av_frame_alloc();
        if (!copied_frame)
        {
            qCWarning(CategoryStreamDecoding, "Frame alloc failed while copy back");
            return AVERROR(ENOMEM);
        }
        if ((ret = av_hwframe_transfer_data(copied_frame.Get(), frame.Get(), 0)) < 0)
        {
            qCWarning(CategoryStreamDecoding, "Copy back failed #%u", ret);
            return ret;
        }

        copied_frame->colorspace = frame->colorspace;
        copied_frame->color_range = frame->color_range;
        frame = std::move(copied_frame);
    }

    bool supported = false;
    switch (frame->format)
    {
    case AV_PIX_FMT_NV12:
    case AV_PIX_FMT_NV21:
    case AV_PIX_FMT_YUV420P:
    case AV_PIX_FMT_YUV444P:
    case AV_PIX_FMT_YUVJ444P:
        supported = true;
        break;
    }

    if (!supported) //Pixel format not supported by shader
    {
        Q_ASSERT(frame->format == video_decoder_ctx_->pix_fmt);
        Q_ASSERT(frame->width == video_decoder_ctx_->width);
        Q_ASSERT(frame->height == video_decoder_ctx_->height);

        sws_context_ = sws_getCachedContext(sws_context_.DetachObject(),
                    frame->width, frame->height, (AVPixelFormat)frame->format,
                    frame->width, frame->height, AV_PIX_FMT_RGB0,
                    SWS_POINT | SWS_BITEXACT, nullptr, nullptr, nullptr);

        int width = frame->width, height = frame->height;

        AVFrameObject converted_frame = av_frame_alloc();
        if (!converted_frame)
        {
            qCWarning(CategoryStreamDecoding, "Frame alloc failed while color format converting");
            return AVERROR(ENOMEM);
        }
        converted_frame->width = width;
        converted_frame->height = height;
        converted_frame->format = AV_PIX_FMT_RGB0;
        ret = av_frame_get_buffer(converted_frame.Get(), 0);
        if (ret < 0)
        {
            qCWarning(CategoryStreamDecoding, "Buffer alloc failed while color format converting");
            return ret;
        }

        ret = sws_scale(sws_context_.Get(), frame->data, frame->linesize, 0, height, converted_frame->data, converted_frame->linesize);
        if (ret < 0)
        {
            qCWarning(CategoryStreamDecoding, "Error while color format converting");
            return ret;
        }
        frame = std::move(converted_frame);
    }

    QSharedPointer<VideoFrame> video_frame = QSharedPointer<VideoFrame>::create();
    video_frame->timestamp = pts;
    video_frame->frame = std::move(frame);

    video_frames_.push_back(std::move(video_frame));

    return 0;
}

int LiveStreamDecoder::ReceiveAudioFrame()
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
        audio_eof_ = true;
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

void LiveStreamDecoder::StartPushTick()
{
    push_tick_time_ = PlaybackClock::now();
    push_tick_enabled_ = true;
#ifdef _DEBUG
    last_debug_report_ = PlaybackClock::now();
#endif
    SetUpNextPushTick();
}

void LiveStreamDecoder::StopPushTick()
{
    push_tick_enabled_ = false;
    push_timer_->stop();
}

void LiveStreamDecoder::OnPushTick()
{
    if (Q_UNLIKELY(!open_))
        return;

    if (!playing() && IsFrameBufferLongerThan(kFrameBufferStartThreshold))
    {
        QMutexLocker lock(&demuxer_out_mutex_);
        if (IsPacketBufferLongerThan(kPacketBufferStartThreshold))
        {
            lock.unlock();
            StartPlaying();
        }
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

        if (Q_LIKELY(!demuxer_eof_)) //No need to lock here, since lock in Decode() should be able to sync this before frame buffer is empty
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
    if (PlaybackClock::now() - last_debug_report_ > std::chrono::seconds(1))
    {
        last_debug_report_ += std::chrono::seconds(1);
        qCDebug(CategoryStreamDecoding) << "Input buffer: %fkiB" << (double)demuxer_in_.SizeLocked() / 1024;
        qCDebug(CategoryStreamDecoding) << "Video packet buffer: " << (video_packets_.empty() ? 0 : AVTimestampToDuration<std::chrono::milliseconds>(video_packets_.back()->pts - video_packets_.front()->pts, video_stream_time_base_).count()) << "ms";
        qCDebug(CategoryStreamDecoding) << "Audio packet buffer: " << (audio_packets_.empty() ? 0ll : AVTimestampToDuration<std::chrono::milliseconds>(audio_packets_.back()->pts - audio_packets_.front()->pts, audio_stream_time_base_).count()) << "ms";
        qCDebug(CategoryStreamDecoding) << "Video frame buffer: " << (video_frames_.empty() ? 0ll : AVTimestampToDuration<std::chrono::milliseconds>(video_frames_.back()->timestamp - video_frames_.front()->timestamp, video_stream_time_base_).count()) << "ms";
        qCDebug(CategoryStreamDecoding) << "Audio frame buffer: " << (audio_frames_.empty() ? 0ll : AVTimestampToDuration<std::chrono::milliseconds>(audio_frames_.back()->timestamp - audio_frames_.front()->timestamp, audio_stream_time_base_).count()) << "ms";
    }
#endif

    Decode(); //Try to pull some packet
    SetUpNextPushTick();
}

void LiveStreamDecoder::SetUpNextPushTick()
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

bool LiveStreamDecoder::IsPacketBufferLongerThan(PlaybackClock::duration duration)
{
    if (video_packets_.empty() || audio_packets_.empty())
        return false;
    if (video_packets_.back()->pts - video_packets_.front()->pts < DurationToAVTimestamp(duration, video_stream_time_base_))
        return false;
    if (audio_packets_.back()->pts - audio_packets_.front()->pts < DurationToAVTimestamp(duration, audio_stream_time_base_))
        return false;
    return true;
}

bool LiveStreamDecoder::IsFrameBufferLongerThan(PlaybackClock::duration duration)
{
    if (video_frames_.empty() || audio_frames_.empty())
        return false;
    if (video_frames_.back()->timestamp - video_frames_.front()->timestamp < DurationToAVTimestamp(duration, video_stream_time_base_))
        return false;
    if (audio_frames_.back()->timestamp - audio_frames_.front()->timestamp < DurationToAVTimestamp(duration, audio_stream_time_base_))
        return false;
    return true;
}

void LiveStreamDecoder::InitPlaying()
{
    video_eof_ = audio_eof_ = false;
}

void LiveStreamDecoder::StartPlaying()
{
    Q_ASSERT(!video_frames_.empty() && !audio_frames_.empty());
    playing_ = true;
    auto current_time = PlaybackClock::now();
    auto video_played_duration = AVTimestampToDuration<std::chrono::microseconds>(video_frames_.front()->timestamp, video_stream_time_base_);
    auto audio_played_duration = AVTimestampToDuration<std::chrono::microseconds>(audio_frames_.front()->timestamp, audio_stream_time_base_);
    auto played_duration = std::max(video_played_duration, audio_played_duration);
    pushed_time_ = std::chrono::duration_cast<std::chrono::milliseconds>(played_duration) + kFrameBufferPushInit;
    base_time_ = current_time - played_duration;
    emit playingChanged(true);
}

void LiveStreamDecoder::StopPlaying()
{
    playing_ = false;
    emit playingChanged(false);
}

void LiveStreamDecoder::StartRecording()
{
    if (!open_)
        return;
    if (remuxer_out_path_default_.isEmpty() && remuxer_out_path_oneshot_.isEmpty())
        return;
    QMutexLocker lock(&remuxer_mutex_);
    if (remuxer_ctx_)
        return;

    AVFormatContextMuxerObject remuxer_ctx;

    QByteArray path;
    if (!remuxer_out_path_oneshot_.isEmpty())
    {
        path = remuxer_out_path_oneshot_.toLocal8Bit();
        remuxer_out_path_oneshot_ = QString();
    }
    else
    {
        path = remuxer_out_path_default_.toLocal8Bit();
    }

    avformat_alloc_output_context2(remuxer_ctx.GetAddressOf(), NULL, NULL, path);
    if (!remuxer_ctx)
        return;

    int stream_index = 0, ret = 0;
    remuxer_stream_map_.clear();
    remuxer_stream_map_.resize(demuxer_ctx_->nb_streams, 0);
    for (size_t i = 0; i < remuxer_stream_map_.size(); ++i)
    {
        AVStream *out_stream;
        AVStream *in_stream = demuxer_ctx_->streams[i];

        AVCodecParameters *in_codecpar = in_stream->codecpar;
        if (in_codecpar->codec_type != AVMEDIA_TYPE_AUDIO && in_codecpar->codec_type != AVMEDIA_TYPE_VIDEO && in_codecpar->codec_type != AVMEDIA_TYPE_SUBTITLE)
        {
            remuxer_stream_map_[i] = -1;
            continue;
        }
        remuxer_stream_map_[i] = stream_index++;

        out_stream = avformat_new_stream(remuxer_ctx.Get(), NULL);
        if (!out_stream)
            return;

        ret = avcodec_parameters_copy(out_stream->codecpar, in_codecpar);
        if (ret < 0)
            return;
        out_stream->codecpar->codec_tag = 0;
    }

    if (!(remuxer_ctx->flags & AVFMT_NOFILE))
    {
        ret = avio_open(&remuxer_ctx->pb, path, AVIO_FLAG_WRITE);
        if (ret < 0)
            return;
    }

    ret = avformat_write_header(remuxer_ctx.Get(), NULL);
    if (ret < 0)
        return;

    remuxer_ctx_ = std::move(remuxer_ctx);
}

void LiveStreamDecoder::StopRecording()
{
    QMutexLocker lock(&remuxer_mutex_); //Already recording
    if (!remuxer_ctx_)
        return;

    av_interleaved_write_frame(remuxer_ctx_.Get(), nullptr); //Flush
    av_write_trailer(remuxer_ctx_.Get());
    remuxer_ctx_ = nullptr;
}

void LiveStreamDecoder::Close()
{
    demuxer_in_.Close();
    StopRecording();
    {
        QMutexLocker lock(&demuxer_out_mutex_);
        demuxer_eof_ = true;
        video_packets_.clear();
        audio_packets_.clear();
    }
    demuxer_out_condition_.notify_all();
    demuxer_thread_.quit();
    demuxer_thread_.wait();

    StopPushTick();
    StopPlaying();
    video_eof_ = audio_eof_ = false;
    video_frames_.clear();
    audio_frames_.clear();
    sws_context_ = nullptr;
    video_decoder_ctx_ = nullptr;
    audio_decoder_ctx_ = nullptr;
    video_decoder_hw_ctx_ = nullptr;
    video_packets_.clear();
    audio_packets_.clear();
    demuxer_eof_ = false;
    demuxer_ctx_ = nullptr;
    input_ctx_ = nullptr;

    if (open_)
    {
        open_ = false;
        emit deleteMedia();
    }
    else
    {
        emit invalidMedia();
    }
}
