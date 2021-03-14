#ifndef AUDIOOUTPUT_H
#define AUDIOOUTPUT_H

#include "AudioFrame.h"

class AudioOutput : public QObject
{
    Q_OBJECT

    struct SwrContextReleaseFunctor
    {
        void operator()(SwrContext **object) const { swr_free(object); }
    };
    using SwrContextObject = AVObjectBase<SwrContext, SwrContextReleaseFunctor>;

    using AudioSourceId = void *;
    using ALBufferId = ALuint;
    using ALSourceId = ALuint;

    struct AudioSource
    {
        static constexpr ALsizei kALBufferCount = 4;

        AudioSource();
        AudioSource(const AudioSource &) = delete;
        AudioSource(AudioSource &&) = delete;
        ~AudioSource();

        AudioSourceId id;
        AVSampleFormat sample_format;
        int channels, sample_channel_size, sample_rate;
        bool force_mono = false;

        std::vector<QSharedPointer<AudioFrame>> pending_frames;
        SwrContextObject swr_context;

        std::vector<uint8_t> buffer_block;
        size_t buffer_block_cap;

        ALSourceId al_id;
        ALenum al_buffer_format;
        ALBufferId al_buffer_occupied[kALBufferCount], al_buffer_free[kALBufferCount];
        ALsizei al_buffer_occupied_count = 0, al_buffer_free_count = 0;

        bool starting = false, muted = false, stopping = false;
    };
public:
    explicit AudioOutput(QObject *parent = nullptr);
    ~AudioOutput();
signals:
    void soloAudioSourceChanged(void *source_id);
public slots:
    //Use void* as a workaround since size_t/uintptr_t causes trouble in signals/slots

    void onNewAudioSource(void *source_id, const AVCodecContext *context);
    void onStopAudioSource(void *source_id);
    void onDeleteAudioSource(void *source_id);

    void onNewAudioFrame(void *source_id, const QSharedPointer<AudioFrame> &audio_frame);
    void onSetAudioSourceVolume(void *source_id, qreal volume);
    void onSetAudioSourcePosition(void *source_id, QVector3D position);
    void onSetAudioSourceMute(void *source_id, bool mute);
    void onSetAudioSourceSolo(void *source_id, bool solo);
private:
    void InitSource(AudioSource &source);
    void InitSource(AudioSource &source, int channels, int64_t channel_layout, AVSampleFormat sample_fmt, int sample_rate);
    AudioSource *GetOrCreateSource(void *source_id);

    void StartSource(const std::shared_ptr<AudioSource> &source, PlaybackClock::time_point timestamp);
    static void AppendFrameToSourceBuffer(AudioSource &source, const QSharedPointer<AudioFrame> &audio_frame);
    static void AppendBufferToSource(AudioSource &source);
    static void CollectExhaustedBuffer(AudioSource &source);
    static void StopSource(AudioSource &source);

    ALCdevice *device_ = nullptr;
    ALCcontext *context_ = nullptr;
    LPALGETSOURCEI64VSOFT alGetSourcei64vSOFT;
    std::unordered_map<AudioSourceId, std::shared_ptr<AudioSource>> sources_;

    void *solo_source_id_ = nullptr;
};

#endif // AUDIOOUTPUT_H
