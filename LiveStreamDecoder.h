#ifndef LIVESTREAMDECODER_H
#define LIVESTREAMDECODER_H

#include "VideoFrame.h"
#include "AudioFrame.h"
#include "SubtitleFrame.h"

#include "BlockingFIFOBuffer.h"

class LiveStreamDecoder;

class LiveStreamSourceDemuxWorker : public QObject
{
    Q_OBJECT

public:
    explicit LiveStreamSourceDemuxWorker(LiveStreamDecoder *decoder) :QObject(nullptr), decoder_(decoder) {}
public slots:
    void Work();
private:
    LiveStreamDecoder *decoder_ = nullptr;
};

class LiveStreamDecoder : public QObject
{
    Q_OBJECT

    struct AVPacketObject
    {
    public:
        AVPacketObject() = default;
        AVPacketObject(const AVPacketObject &obj) = delete;
        AVPacketObject(AVPacketObject &&obj)
        {
            if (!obj.owns_object)
                return;
            av_packet_move_ref(&object, &obj.object);
            obj.owns_object = false;
            owns_object = true;
        }
        AVPacketObject &operator=(const AVPacketObject &obj) = delete;
        AVPacketObject &operator=(AVPacketObject &&obj)
        {
            if (this == &obj)
                return *this;
            if (owns_object)
            {
                av_packet_unref(&object);
                owns_object = false;
            }
            if (!obj.owns_object)
                return *this;
            av_packet_move_ref(&object, &obj.object);
            obj.owns_object = false;
            owns_object = true;
            return *this;
        }
        ~AVPacketObject()
        {
            if (owns_object)
                av_packet_unref(&object);
        }

        AVPacket *Get() { return &object; }
        AVPacket *ReleaseAndGet()
        {
            if (owns_object)
            {
                av_packet_unref(&object);
                owns_object = false;
            }
            return &object;
        }
        void SetOwn() { owns_object = true; }
        void Release()
        {
            if (owns_object)
            {
                av_packet_unref(&object);
                owns_object = false;
            }
        }

        const AVPacket &operator*() const { return object; }
        AVPacket &operator*() { return object; }
        const AVPacket *operator->() const { return &object; }
        AVPacket *operator->() { return &object; }
        operator bool() const { return owns_object; }
        bool operator!() const { return !owns_object; }
    private:
        AVPacket object;
        bool owns_object = false;
    };

    struct AVAllocatedMemoryReleaseFunctor
    {
        void operator()(uint8_t **object) const { uint8_t *p = *object; *object = nullptr; av_free(p); }
    };
    using AVAllocatedMemory = AVObjectBase<uint8_t, AVAllocatedMemoryReleaseFunctor>;
    struct AVIOContextReleaseFunctor
    {
        void operator()(AVIOContext **object) const { uint8_t *buffer = (*object)->buffer; avio_context_free(object); av_free(buffer); }
    };
    using AVIOContextObject = AVObjectBase<AVIOContext, AVIOContextReleaseFunctor>;
    struct AVFormatContextDemuxerReleaseFunctor
    {
        void operator()(AVFormatContext **object) const { avformat_close_input(object); }
    };
    using AVFormatContextDemuxerObject = AVObjectBase<AVFormatContext, AVFormatContextDemuxerReleaseFunctor>;
    struct AVFormatContextMuxerReleaseFunctor
    {
        void operator()(AVFormatContext **object) const { AVFormatContext *p = *object; *object = nullptr; if (p && !(p->flags & AVFMT_NOFILE)) avio_closep(&p->pb); avformat_free_context(p); }
    };
    using AVFormatContextMuxerObject = AVObjectBase<AVFormatContext, AVFormatContextMuxerReleaseFunctor>;
    struct AVCodecContextReleaseFunctor
    {
        void operator()(AVCodecContext **object) const { avcodec_free_context(object); }
    };
    using AVCodecContextObject = AVObjectBase<AVCodecContext, AVCodecContextReleaseFunctor>;
    struct SwsContextReleaseFunctor
    {
        void operator()(SwsContext **object) const { SwsContext *p = *object; *object = nullptr; sws_freeContext(p); }
    };
    using SwsContextObject = AVObjectBase<SwsContext, SwsContextReleaseFunctor>;

public:
    explicit LiveStreamDecoder(QObject *parent = nullptr);
    ~LiveStreamDecoder();

    bool open() const { return open_; }
    bool playing() const { return playing_; }

    void BeginData();
    size_t PushData(const char *data, size_t size);
    void PushData(QIODevice *device);
    void EndData();
    void CloseData();
signals:
    void playingChanged(bool new_playing);

    void invalidMedia();
    void newMedia(const AVCodecContext *video_decoder_context, const AVCodecContext *audio_decoder_context);
    void newVideoFrame(const QSharedPointer<VideoFrame> &video_frame);
    void newAudioFrame(const QSharedPointer<AudioFrame> &audio_frame);
    void deleteMedia();
public slots:
    void onNewInputStream(const QString &url_hint, const QString &record_path);
    void onDeleteInputStream();
    void onSetDefaultMediaRecordFile(const QString &file_path);
    void onSetOneshotMediaRecordFile(const QString &file_path);
private slots:
    void OnPushTick();
private:
    friend class LiveStreamSourceDemuxWorker;
    static int AVIOReadCallback(void *opaque, uint8_t *buf, int buf_size);

    void Decode();
    int SendVideoPacket(AVPacket &packet);
    int SendAudioPacket(AVPacket &packet);
    int ReceiveVideoFrame();
    int ReceiveAudioFrame();

    void StartPushTick();
    void StopPushTick();
    void SetUpNextPushTick();

    bool IsPacketBufferLongerThan(PlaybackClock::duration duration);
    bool IsFrameBufferLongerThan(PlaybackClock::duration duration);
    void InitPlaying();
    void StartPlaying();
    void StopPlaying();

    void StartRecording();
    void StopRecording();

    void Close();

    AVIOContextObject input_ctx_;

    QThread demuxer_thread_;

    BlockingFIFOBuffer demuxer_in_;

    AVFormatContextDemuxerObject demuxer_ctx_;
    int video_stream_index_, audio_stream_index_;

    QString remuxer_out_path_default_, remuxer_out_path_oneshot_;
    QMutex remuxer_mutex_;
    AVFormatContextMuxerObject remuxer_ctx_;
    std::vector<int> remuxer_stream_map_;

    QMutex demuxer_out_mutex_;
    QWaitCondition demuxer_out_condition_;
    std::vector<AVPacketObject> video_packets_, audio_packets_;
    bool demuxer_eof_ = false;

    AVBufferRefObject video_decoder_hw_ctx_;
    AVCodecContextObject video_decoder_ctx_, audio_decoder_ctx_;
    AVPixelFormat video_decoder_hw_pixel_format_;
    SwsContextObject sws_context_;

    std::vector<QSharedPointer<VideoFrame>> video_frames_;
    std::vector<QSharedPointer<AudioFrame>> audio_frames_;
    bool open_ = false, playing_ = false, video_eof_ = false, audio_eof_ = false;

    AVRational video_stream_time_base_, audio_stream_time_base_;
    PlaybackClock::time_point base_time_;
    std::chrono::milliseconds pushed_time_;

    QTimer *push_timer_ = nullptr;
    PlaybackClock::time_point push_tick_time_;
    bool push_tick_enabled_ = false;

#ifdef _DEBUG
    PlaybackClock::time_point last_debug_report_;
#endif
};

#endif // LIVESTREAMDECODER_H
