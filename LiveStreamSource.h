#ifndef LIVESTREAMSOURCE_H
#define LIVESTREAMSOURCE_H

#include "VideoFrame.h"
#include "AudioFrame.h"
#include "SubtitleFrame.h"

using SourceInputCallback = int(*)(void *opaque, uint8_t *buf, int buf_size);
Q_DECLARE_METATYPE(SourceInputCallback);

class LiveStreamView;

class LiveStreamSource : public QObject
{
    Q_OBJECT

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
    explicit LiveStreamSource(QObject *parent = nullptr);
    ~LiveStreamSource();

    bool open() const { return open_; }
    bool playing() const { return playing_; }
signals:
    void playingChanged();

    void newMedia(const AVCodecContext *video_decoder_context, const AVCodecContext *audio_decoder_context);
    void newVideoFrame(QSharedPointer<VideoFrame> video_frame);
    void newAudioFrame(QSharedPointer<AudioFrame> audio_frame);
    void newSubtitleFrame(QSharedPointer<SubtitleFrame> subtitle_frame);
    void deleteMedia();

    void queuePushTick();
public slots:
    void OnNewInputStream(void *opaque, SourceInputCallback read_callback);
    void OnNewInputDataDeady();
private slots:
    void OnPushTick();
private:
    int ReceiveVideoFrame();
    int ReceiveAudioFrame();

    void StartPushTick();
    void StopPushTick();
    void SetUpNextPushTick();

    bool IsBufferLongerThan(PlaybackClock::duration duration);
    void InitPlaying();
    void StartPlaying();
    void StopPlaying();

    void Close();

    AVIOContextObject input_ctx_;
    AVFormatContextObject demuxer_ctx_;
    int video_stream_index_, audio_stream_index_;
    AVCodecContextObject video_decoder_ctx_, audio_decoder_ctx_;
    SwsContextObject sws_context_;
    bool open_ = false;

    bool playing_ = false;
    std::vector<QSharedPointer<VideoFrame>> video_frames_;
    std::vector<QSharedPointer<AudioFrame>> audio_frames_;

    AVRational video_stream_time_base_, audio_stream_time_base_;
    PlaybackClock::time_point base_time_;
    std::chrono::milliseconds pushed_time_;

    PlaybackClock::time_point push_tick_time_;
    bool push_tick_enabled_ = false;
#ifdef _DEBUG
    int video_frames_per_second_ = 0;
#endif
};

#endif // LIVESTREAMSOURCE_H
