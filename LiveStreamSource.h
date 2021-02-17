#ifndef LIVESTREAMSOURCE_H
#define LIVESTREAMSOURCE_H

#include "VideoFrame.h"

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

    static constexpr PlaybackClock::duration kDecodeToRenderLatency = std::chrono::milliseconds(500);

    //Q_PROPERTY(int sourceId READ sourceId WRITE setSourceId NOTIFY sourceIdChanged)
public:
    explicit LiveStreamSource(QObject *parent = nullptr);

    Q_INVOKABLE void start();
signals:
    void newMedia();
    void newVideoFrame(QSharedPointer<VideoFrame> video_frame);
    //void newAudioFrame();

    void queueNextVideoFrameTick();
    void temporaryRefreshSignal();
public slots:
    void onNextVideoFrameTick();
private:
    void SendData();
    void ReceiveVideoFrame();
    void SetUpNextVideoFrameTick();

    AVFormatContextObject input_ctx;
    int video_stream_index, audio_stream_index;
    AVCodecContextObject video_decoder_ctx;
    AVPacket packet;
    SwsContextObject sws_context_;

    PlaybackClock::time_point next_frame_time_;
    PlaybackClock::duration frame_rate_, next_frame_offset_;
    int frames_per_second_ = 0;
};

#endif // LIVESTREAMSOURCE_H
