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
    explicit LiveStreamSourceDemuxWorker(LiveStreamDecoder *parent) :QObject(nullptr), parent_(parent) {}
public slots:
    void Work();
private:
    LiveStreamDecoder *parent_ = nullptr;
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

        //AVPacket *Get() { return &object; }
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
        void operator()(uint8_t **object) const { av_free(*object); *object = nullptr; }
    };
    using AVAllocatedMemory = AVObjectBase<uint8_t, AVAllocatedMemoryReleaseFunctor>;
    struct AVIOContextReleaseFunctor
    {
        void operator()(AVIOContext **object) const { uint8_t *buffer = (*object)->buffer; avio_context_free(object); av_free(buffer); }
    };
    using AVIOContextObject = AVObjectBase<AVIOContext, AVIOContextReleaseFunctor>;
    struct AVFormatContextReleaseFunctor
    {
        void operator()(AVFormatContext **object) const { avformat_close_input(object); }
    };
    using AVFormatContextObject = AVObjectBase<AVFormatContext, AVFormatContextReleaseFunctor>;
    struct AVCodecContextReleaseFunctor
    {
        void operator()(AVCodecContext **object) const { avcodec_free_context(object); }
    };
    using AVCodecContextObject = AVObjectBase<AVCodecContext, AVCodecContextReleaseFunctor>;
    struct SwsContextReleaseFunctor
    {
        void operator()(SwsContext **object) const { sws_freeContext(*object); *object = nullptr; }
    };
    using SwsContextObject = AVObjectBase<SwsContext, SwsContextReleaseFunctor>;

public:
    explicit LiveStreamDecoder(QObject *parent = nullptr);
    ~LiveStreamDecoder();

    bool open() const { return open_; }
    bool playing() const { return playing_; }

    size_t PushData(const char *data, size_t size);
    void PushData(QIODevice *device);
    void EndData();
signals:
    void playingChanged(bool new_playing);

    void invalidMedia();
    void newMedia(const AVCodecContext *video_decoder_context, const AVCodecContext *audio_decoder_context);
    void newVideoFrame(QSharedPointer<VideoFrame> video_frame);
    void newAudioFrame(QSharedPointer<AudioFrame> audio_frame);
    void deleteMedia();
public slots:
    void onNewInputStream(const QString &url_hint);
    void onDeleteInputStream();
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

    void Close();

    AVIOContextObject input_ctx_;

    QThread demuxer_thread_;
    BlockingFIFOBuffer demuxer_in_;
    AVFormatContextObject demuxer_ctx_;
    QMutex demuxer_out_mutex_;
    QWaitCondition demuxer_out_condition_;
    bool demuxer_eof_ = false;
    std::vector<AVPacketObject> video_packets_, audio_packets_;

    int video_stream_index_, audio_stream_index_;

    AVCodecContextObject video_decoder_ctx_, audio_decoder_ctx_;
    SwsContextObject sws_context_;
    bool open_ = false;

    bool playing_ = false, video_eof_ = false, audio_eof_ = false;
    std::vector<QSharedPointer<VideoFrame>> video_frames_;
    std::vector<QSharedPointer<AudioFrame>> audio_frames_;

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
