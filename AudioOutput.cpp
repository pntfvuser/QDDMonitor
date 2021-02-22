#include "pch.h"
#include "AudioOutput.h"

static constexpr auto kFallbackLatencyAssumption = std::chrono::milliseconds(60);

AudioOutput::AudioSource::AudioSource()
{
    ALenum ret;

    alGenSources(1, &al_id);
    if ((ret = alGetError()) != AL_NO_ERROR)
    {
        throw std::runtime_error("Failed to create source");
    }

    alGenBuffers(AudioSource::kALBufferCount, al_buffer_free);
    if ((ret = alGetError()) != AL_NO_ERROR)
    {
        alDeleteSources(1, &al_id);
        throw std::runtime_error("Failed to create source");
    }

    al_buffer_free_count = AudioSource::kALBufferCount;
}

AudioOutput::AudioSource::~AudioSource()
{
    alSourceStop(al_id);
    alSourceUnqueueBuffers(al_id, al_buffer_occupied_count, al_buffer_occupied);
    alDeleteSources(1, &al_id);
    alDeleteBuffers(al_buffer_occupied_count, al_buffer_occupied);
    alDeleteBuffers(al_buffer_free_count, al_buffer_free);
}

AudioOutput::AudioOutput(QQuickItem *parent)
    :QQuickItem(parent)
{
    device_ = alcOpenDevice(nullptr);
    Q_ASSERT(device_);
    context_ = alcCreateContext(device_, nullptr);
    Q_ASSERT(context_);
    alcMakeContextCurrent(context_);
    alGetError();

    alGetSourcei64vSOFT = (LPALGETSOURCEI64VSOFT)alGetProcAddress("alGetSourcei64vSOFT");
}

AudioOutput::~AudioOutput()
{
    sources_.clear();
    if (context_)
    {
        alcMakeContextCurrent(nullptr);
        alcDestroyContext(context_);
    }
    if (device_)
    {
        alcCloseDevice(device_);
    }
}

void AudioOutput::onNewAudioSource(uintptr_t source_id, const AVCodecContext *context)
{
    std::shared_ptr<AudioSource> new_source;
    try
    {
        new_source = std::make_shared<AudioSource>();
    }
    catch (...)
    {
        qWarning("Failed to create new AudioSource");
        return;
    }
    AudioSource &source = *sources_.emplace(source_id, std::move(new_source)).first->second;
    source.id = source_id;

    InitSource(source, context->channels, context->channel_layout, context->sample_fmt, context->sample_rate);
}

void AudioOutput::onDeleteAudioSource(uintptr_t source_id)
{
    auto itr = sources_.find(source_id);
    if (itr == sources_.end())
        return;
    itr->second->stopping = true;
    sources_.erase(itr);
}

void AudioOutput::onNewAudioFrame(uintptr_t source_id, QSharedPointer<AudioFrame> audio_frame)
{
    AudioSource *source;

    auto itr = sources_.find(source_id);
    if (itr == sources_.end())
    {
        std::shared_ptr<AudioSource> new_source;
        try
        {
            new_source = std::make_shared<AudioSource>();
        }
        catch (...)
        {
            qWarning("Failed to create new AudioSource");
            return;
        }
        itr = sources_.emplace(source_id, std::move(new_source)).first;
        source = itr->second.get();
        source->id = source_id;
        InitSource(*source, audio_frame->frame->channels, audio_frame->frame->channel_layout, audio_frame->sample_format, audio_frame->frame->sample_rate);
    }
    else
    {
        source = itr->second.get();
        if (source->channels != audio_frame->frame->channels || source->sample_format != audio_frame->sample_format || source->sample_rate != audio_frame->frame->sample_rate)
        {
            InitSource(*source, audio_frame->frame->channels, audio_frame->frame->channel_layout, audio_frame->sample_format, audio_frame->frame->sample_rate);
            //Resynchronize
            source->pending_frames.clear();
            source->buffer_block.clear();
            alSourceStop(source->al_id);
            CollectExhaustedBuffer(*source);
        }
    }

    ALenum ret = AL_NO_ERROR;
    ALint source_state = AL_STOPPED;
    alGetSourcei(source->al_id, AL_SOURCE_STATE, &source_state);
    if ((ret = alGetError()) != AL_NO_ERROR)
    {
        qWarning("Can't get source state #%d", ret);
    }
    const bool need_start = source_state != AL_PLAYING && !source->starting;
    PlaybackClock::time_point need_start_time;
    if (need_start)
    {
        need_start_time = audio_frame->present_time;
    }

    CollectExhaustedBuffer(*source);
    while (source->buffer_block.size() < source->buffer_block_cap && !source->pending_frames.empty())
    {
        AppendFrameToSourceBuffer(*source, source->pending_frames.front());
        if (source->buffer_block.size() >= source->buffer_block_cap)
        {
            AppendBufferToSource(*source);
        }
        source->pending_frames.erase(source->pending_frames.begin());
    }
    if (source->buffer_block.size() < source->buffer_block_cap)
    {
        AppendFrameToSourceBuffer(*source, audio_frame);
        if (source->buffer_block.size() >= source->buffer_block_cap)
        {
            AppendBufferToSource(*source);
        }
    }
    else
    {
        source->pending_frames.push_back(std::move(audio_frame));
    }

    if (need_start)
    {
        StartSource(itr->second, need_start_time);
    }
}

void AudioOutput::onSetAudioSourceVolume(uintptr_t source_id, qreal volume)
{
    AudioSource *source;

    auto itr = sources_.find(source_id);
    if (itr == sources_.end())
    {
        std::shared_ptr<AudioSource> new_source;
        try
        {
            new_source = std::make_shared<AudioSource>();
        }
        catch (...)
        {
            qWarning("Failed to create new AudioSource");
            return;
        }
        itr = sources_.emplace(source_id, std::move(new_source)).first;
        source = itr->second.get();
        source->id = source_id;
        InitSource(*source);
    }
    else
    {
        source = itr->second.get();
    }

    if (volume < 0)
        volume = 0;
    else if (volume > 1)
        volume = 1;
    alSourcef(source->al_id, AL_GAIN, (ALfloat)volume);
}

void AudioOutput::InitSource(AudioOutput::AudioSource &source)
{
    source.channels = 0;
    source.sample_format = AV_SAMPLE_FMT_NONE;
    source.sample_rate = 0;
    source.buffer_block_cap = 0;
}

void AudioOutput::InitSource(AudioSource &source, int channels, int64_t channel_layout, AVSampleFormat sample_fmt, int sample_rate)
{
    static constexpr int kBufferBlockSizeMS = 100;

    source.channels = channels;
    source.sample_format = sample_fmt;
    source.sample_rate = sample_rate;

    int out_channels = 0;
    AVSampleFormat out_sample_format = AV_SAMPLE_FMT_NONE;
    switch (channels)
    {
    case 1:
    case 2:
        out_channels = channels;
        break;
    default:
        out_channels = 2;
        break;
    }
    switch (sample_fmt)
    {
    case AV_SAMPLE_FMT_U8:
    case AV_SAMPLE_FMT_S16:
        out_sample_format = sample_fmt;
        break;
    default:
        out_sample_format = AV_SAMPLE_FMT_S16;
        break;
    }
    if (out_channels != channels || out_sample_format != sample_fmt)
    {
        int64_t out_channel_layout;
        if (out_channels == 2)
        {
            out_channel_layout = AV_CH_LAYOUT_STEREO;
        }
        else
        {
            out_channel_layout = AV_CH_LAYOUT_MONO;
        }
        source.swr_context = swr_alloc_set_opts(nullptr,
                                                out_channel_layout, out_sample_format, sample_rate,
                                                channel_layout,     sample_fmt,        sample_rate,
                                                0, nullptr);
        swr_init(source.swr_context.Get());
    }
    if (out_sample_format == AV_SAMPLE_FMT_S16)
    {
        source.sample_channel_size = 2 * out_channels;
        if (out_channels == 2)
            source.al_buffer_format = AL_FORMAT_STEREO16;
        else //Q_ASSERT(out_channels == 1)
            source.al_buffer_format = AL_FORMAT_MONO16;
    }
    else //Q_ASSERT(out_sample_format == AV_SAMPLE_FMT_U8)
    {
        source.sample_channel_size = 1 * out_channels;
        if (out_channels == 2)
            source.al_buffer_format = AL_FORMAT_STEREO16;
        else //Q_ASSERT(out_channels == 1)
            source.al_buffer_format = AL_FORMAT_MONO16;
    }

    source.buffer_block_cap = source.sample_rate * kBufferBlockSizeMS / 1000 * source.sample_channel_size;
}

void AudioOutput::StartSource(const std::shared_ptr<AudioSource> &source, PlaybackClock::time_point timestamp)
{
    std::chrono::nanoseconds latency = kFallbackLatencyAssumption;

    ALenum ret = AL_NO_ERROR;
    ALint64SOFT offset_latency[2];
    alGetSourcei64vSOFT(source->al_id, AL_SAMPLE_OFFSET_LATENCY_SOFT, offset_latency);
    if ((ret = alGetError()) != AL_NO_ERROR)
    {
        qWarning("Can't get precise latency #%d", ret);
    }
    else
    {
        qDebug("Latency: %lldns", offset_latency[1]);
        latency = std::chrono::nanoseconds(offset_latency[1]);
    }

    std::chrono::milliseconds sleep_time = std::chrono::duration_cast<std::chrono::milliseconds>(timestamp - PlaybackClock::now() - latency);
    qDebug("Starting audio playback after %lldms", sleep_time.count());
    if (sleep_time.count() <= 0)
    {
        alSourcePlay(source->al_id);
    }
    else
    {
        QTimer::singleShot(sleep_time, this, [source_weak = std::weak_ptr<AudioSource>(source)]()
        {
            auto source = source_weak.lock();
            if (!source)
                return;
            if (!source->stopping)
            {
                source->starting = false;
                alSourcePlay(source->al_id);
                ALenum ret = AL_NO_ERROR;
                if ((ret = alGetError()) != AL_NO_ERROR)
                {
                    qWarning("Can't start playback #%d", ret);
                }
            }
        });
        source->starting = true;
    }
}

void AudioOutput::AppendFrameToSourceBuffer(AudioSource &source, const QSharedPointer<AudioFrame> &audio_frame)
{
    if (source.swr_context)
    {
        int in_size = audio_frame->frame->nb_samples;
        int out_size_est = swr_get_out_samples(source.swr_context.Get(), in_size);
        size_t out_offset = source.buffer_block.size();
        source.buffer_block.resize(out_offset + out_size_est * source.sample_channel_size);
        uint8_t *out[1] = { source.buffer_block.data() + out_offset };
        int out_size = swr_convert(source.swr_context.Get(), out, out_size_est, (const uint8_t **)audio_frame->frame->data, in_size);
        Q_ASSERT(out_size >= 0);
        source.buffer_block.resize(out_offset + out_size * source.sample_channel_size);
    }
    else
    {
        int in_size_in_bytes = audio_frame->frame->nb_samples * source.sample_channel_size;
        size_t out_offset = source.buffer_block.size();
        source.buffer_block.resize(out_offset + in_size_in_bytes);
        memcpy(source.buffer_block.data() + out_offset, audio_frame->frame->data[0], in_size_in_bytes);
    }
}

void AudioOutput::AppendBufferToSource(AudioSource &source)
{
    while (source.buffer_block.size() >= source.buffer_block_cap && source.al_buffer_free_count > 0)
    {
        ALenum ret = AL_NO_ERROR;
        ALBufferId buffer_id = source.al_buffer_free[--source.al_buffer_free_count];
        alBufferData(buffer_id, source.al_buffer_format, source.buffer_block.data(), source.buffer_block_cap, source.sample_rate);
        if ((ret = alGetError()) != AL_NO_ERROR)
        {
            qWarning("Can't specify OpenAL buffer content #%d", ret);
        }
        alSourceQueueBuffers(source.al_id, 1, &buffer_id);
        if ((ret = alGetError()) != AL_NO_ERROR)
        {
            qWarning("Can't append buffer to queue #%d", ret);
        }
        source.al_buffer_occupied[source.al_buffer_occupied_count++] = buffer_id;
        source.buffer_block.erase(source.buffer_block.begin(), source.buffer_block.begin() + source.buffer_block_cap);
    }
}

void AudioOutput::CollectExhaustedBuffer(AudioSource &source)
{
    if (source.starting)
        return;
    ALint buffers_processed = 0;
    alGetSourcei(source.al_id, AL_BUFFERS_PROCESSED, &buffers_processed);
    if (alGetError() != AL_NO_ERROR)
    {
        qWarning("Can't get number of exhausted buffer");
    }
    if (buffers_processed > 0)
    {
        ALsizei before = source.al_buffer_free_count;
        Q_ASSERT(before + buffers_processed <= AudioSource::kALBufferCount);
        alSourceUnqueueBuffers(source.al_id, buffers_processed, source.al_buffer_free + before);
        if (alGetError() != AL_NO_ERROR)
        {
            qWarning("Can't unqueue buffer");
        }
        ALsizei after = before + buffers_processed;

        ALsizei i, p = 0;
        for (i = 0; i < source.al_buffer_occupied_count; ++i)
        {
            bool freed = false;
            for (ALsizei j = before; j < after; ++j)
            {
                if (source.al_buffer_occupied[i] == source.al_buffer_free[j])
                {
                    freed = true;
                    break;
                }
            }
            if (!freed)
            {
                source.al_buffer_occupied[p++] = source.al_buffer_occupied[i];
            }
        }

        source.al_buffer_free_count = after;
        source.al_buffer_occupied_count = p;
        Q_ASSERT(source.al_buffer_occupied_count + source.al_buffer_free_count == AudioSource::kALBufferCount);
    }
}
