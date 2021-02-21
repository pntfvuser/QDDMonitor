#ifndef LIVESTREAMSOURCE_H
#define LIVESTREAMSOURCE_H

#include "VideoFrame.h"
#include "AudioFrame.h"

class LiveStreamView;

class LiveStreamSource : public QObject
{
    Q_OBJECT

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
        void operator()(SwsContext **object) const { sws_freeContext(*object); }
    };
    using SwsContextObject = AVObjectBase<SwsContext, SwsContextReleaseFunctor>;
public:
    explicit LiveStreamSource(QObject *parent = nullptr);

    bool playing() const { return playing_; }

    Q_INVOKABLE void start();
signals:
    void playingChanged();

    void newMedia(const AVCodecContext *video_decoder_context, const AVCodecContext *audio_decoder_context);
    void newVideoFrame(QSharedPointer<VideoFrame> video_frame);
    void newAudioFrame(QSharedPointer<AudioFrame> audio_frame);

    void queuePushTick();
public slots:
    void onPushTick();

    void debugSourceTick();
private:
    bool IsBufferLongerThan(PlaybackClock::duration duration);
    void StartPlaying();
    void StopPlaying();

    void Synchronize();

    int ReceiveVideoFrame();
    int ReceiveAudioFrame();

    void StartPushTick();
    void StopPushTick();
    void SetUpNextPushTick();

    AVFormatContextObject input_ctx_;
    int video_stream_index_, audio_stream_index_;
    AVCodecContextObject video_decoder_ctx_, audio_decoder_ctx_;
    SwsContextObject sws_context_;

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
