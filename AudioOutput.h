#ifndef AUDIOOUTPUT_H
#define AUDIOOUTPUT_H

#include "AudioFrame.h"

class AudioOutput : public QQuickItem
{
    Q_OBJECT

    struct SwrContextReleaseFunctor
    {
        void operator()(SwrContext **object) const { swr_free(object); }
    };
    using SwrContextObject = AVObjectBase<SwrContext, SwrContextReleaseFunctor>;

    using ALBufferId = ALuint;
    using ALSourceId = ALuint;

    struct AudioSource
    {
        static constexpr ALsizei kALBufferCount = 4;

        AudioSource();
        AudioSource(const AudioSource &) = delete;
        AudioSource(AudioSource &&) = delete;
        ~AudioSource();

        int id;
        AVSampleFormat sample_format;
        int channels, sample_channel_size, sample_rate;

        std::vector<QSharedPointer<AudioFrame>> pending_frames;
        SwrContextObject swr_context;

        std::vector<uint8_t> buffer_block;
        size_t buffer_block_cap;

        ALSourceId al_id;
        ALenum al_buffer_format;
        ALBufferId al_buffer_occupied[kALBufferCount], al_buffer_free[kALBufferCount];
        ALsizei al_buffer_occupied_count = 0, al_buffer_free_count = 0;

        bool starting = false, stopping = false;
    };

public:
    explicit AudioOutput(QQuickItem *parent = nullptr);
    ~AudioOutput();

signals:

public slots:
    void onNewAudioSource(int source_id, const AVCodecContext *context);
    void onDeleteAudioSource(int source_id);

    void onNewAudioFrame(int source_id, QSharedPointer<AudioFrame> audio_frame);
    void onSetAudioSourceVolume(int source_id, qreal volume);
private:
    void InitSource(AudioSource &source);
    void InitSource(AudioSource &source, int channels, int64_t channel_layout, AVSampleFormat sample_fmt, int sample_rate);

    void StartSource(const std::shared_ptr<AudioSource> &source, PlaybackClock::time_point timestamp);
    static void AppendFrameToSourceBuffer(AudioSource &source, const QSharedPointer<AudioFrame> &audio_frame);
    static void AppendBufferToSource(AudioSource &source);
    static void CollectExhaustedBuffer(AudioSource &source);

    ALCdevice *device_ = nullptr;
    ALCcontext *context_ = nullptr;
    LPALGETSOURCEI64VSOFT alGetSourcei64vSOFT;
    std::unordered_map<int, std::shared_ptr<AudioSource>> sources_;
};

#endif // AUDIOOUTPUT_H
