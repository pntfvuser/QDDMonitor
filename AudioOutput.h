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
        static constexpr ALsizei kALBufferCount = 8;

        AudioSource() { alGenSources(1, &al_id); }
        AudioSource(const AudioSource &) = delete;
        AudioSource(AudioSource &&) = delete;
        ~AudioSource()
        {
            alSourceStop(al_id);
            alSourceUnqueueBuffers(al_id, al_buffer_occupied_count, al_buffer_occupied);
            alDeleteSources(1, &al_id);
            alDeleteBuffers(al_buffer_occupied_count, al_buffer_occupied);
            alDeleteBuffers(al_buffer_free_count, al_buffer_free);
        }

        uintptr_t id;

        std::vector<QSharedPointer<AudioFrame>> pending_frames;
        SwrContextObject swr_context;
        std::vector<uint8_t> buffer_block;
        size_t buffer_block_cap;
        int buffer_sample_channel_size, buffer_sample_rate;

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
    void onNewAudioSource(uintptr_t source_id, const AVCodecContext *context);
    void onDeleteAudioSource(uintptr_t source_id);

    void onNewAudioFrame(uintptr_t source_id, QSharedPointer<AudioFrame> audio_frame);
private:
    void StartSource(const std::shared_ptr<AudioSource> &source, PlaybackClock::time_point timestamp);
    static void StartSourceTimerCallback(const std::shared_ptr<AudioSource> &source);
    void AppendFrameToSourceBuffer(const std::shared_ptr<AudioSource> &source, const QSharedPointer<AudioFrame> &audio_frame);
    void AppendBufferToSource(const std::shared_ptr<AudioSource> &source);
    void CollectExhaustedBuffer(const std::shared_ptr<AudioSource> &source);

    ALCdevice *device_ = nullptr;
    ALCcontext *context_ = nullptr;
    LPALGETSOURCEI64VSOFT alGetSourcei64vSOFT;
    std::unordered_map<uintptr_t, std::shared_ptr<AudioSource>> sources_;
};

#endif // AUDIOOUTPUT_H
