#ifndef AUDIOOUTPUT_H
#define AUDIOOUTPUT_H

#include "AudioFrame.h"

class AudioOutputBuffer;

class AudioOutput : public QQuickItem
{
    Q_OBJECT

    struct SwrContextReleaseFunctor
    {
        void operator()(SwrContext **object) const { swr_free(object); }
    };
    using SwrContextObject = AVObjectBase<SwrContext, SwrContextReleaseFunctor>;

    struct AudioSource
    {
        uintptr_t id;
        SwrContextObject swr_context;
        int64_t sample_position;
    };

public:
    explicit AudioOutput(QQuickItem *parent = nullptr);

signals:

public slots:
    void onNewAudioSource(uintptr_t source_id, const AVCodecContext *context);
    void onDeleteAudioSource(uintptr_t source_id);

    void onNewAudioFrame(uintptr_t source_id, QSharedPointer<AudioFrame> audio_frame);
private:
    void InitNewSource();

    std::unordered_map<uintptr_t, AudioSource> sources_;
    AudioOutputBuffer *buffer_ = nullptr;
    QAudioOutput *output_ = nullptr;

    unsigned int channel_layout_ = 0;
    size_t sample_size_ = 0;
    AVSampleFormat sample_format_ = AV_SAMPLE_FMT_NONE;
    int sample_rate_ = 0;
    QAudioFormat::Endian sample_endian_;
};

#endif // AUDIOOUTPUT_H
