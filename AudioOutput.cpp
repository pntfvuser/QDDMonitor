#include "pch.h"
#include "AudioOutput.h"

enum SampleSizeSupport
{
    SAMPLE_SIZE_8 = 0x01,
    SAMPLE_SIZE_16 = 0x02,
    SAMPLE_SIZE_32 = 0x04,
    SAMPLE_SIZE_64 = 0x08,
};

enum SampleTypeSupport
{
    SAMPLE_TYPE_FLOAT = 0x01,
    SAMPLE_TYPE_SINT = 0x02,
    SAMPLE_TYPE_UINT = 0x04,
};

static AVSampleFormat GetAVSampleFormat(unsigned int sample_size_support, unsigned int sample_type_support)
{
    if (sample_size_support & SAMPLE_SIZE_64)
    {
        if (sample_type_support & SAMPLE_TYPE_SINT)
        {
            return AV_SAMPLE_FMT_S64;
        }
        else if (sample_type_support & SAMPLE_TYPE_FLOAT)
        {
            return AV_SAMPLE_FMT_DBL;
        }
    }
    if (sample_size_support & SAMPLE_SIZE_32)
    {
        if (sample_type_support & SAMPLE_TYPE_SINT)
        {
            return AV_SAMPLE_FMT_S32;
        }
        else if (sample_type_support & SAMPLE_TYPE_FLOAT)
        {
            return AV_SAMPLE_FMT_FLT;
        }
    }
    if (sample_size_support & SAMPLE_SIZE_16)
    {
        if (sample_type_support & SAMPLE_TYPE_SINT)
        {
            return AV_SAMPLE_FMT_S16;
        }
    }
    if (sample_size_support & SAMPLE_SIZE_8)
    {
        if (sample_type_support & SAMPLE_TYPE_UINT)
        {
            return AV_SAMPLE_FMT_U8;
        }
    }
    return AV_SAMPLE_FMT_NONE;
}

static unsigned int GetSampleSizeSupportFlag(int sample_size)
{
    switch (sample_size)
    {
    case 8:
        return SAMPLE_SIZE_8;
    case 16:
        return SAMPLE_SIZE_16;
    case 32:
        return SAMPLE_SIZE_32;
    case 64:
        return SAMPLE_SIZE_64;
    default:
        return 0;
    }
}

static unsigned int GetSampleTypeSupportFlag(QAudioFormat::SampleType sample_type)
{
    switch (sample_type)
    {
    case QAudioFormat::SignedInt:
        return SAMPLE_TYPE_SINT;
    case QAudioFormat::UnSignedInt:
        return SAMPLE_TYPE_UINT;
    case QAudioFormat::Float:
        return SAMPLE_TYPE_FLOAT;
    default:
        return 0;
    }
}

class AudioOutputBuffer final : public QIODevice
{
    static constexpr size_t kBufferChunkSize = 0x40000;
    struct BufferChunk
    {
        uint8_t sample_data[kBufferChunkSize];
    };
public:
    explicit AudioOutputBuffer(AudioOutput *parent) :QIODevice(parent)
    {
        active_chunks_.emplace_back();
        back_pos_ = kBufferChunkSize / 2;
    }

    virtual bool isSequential() const override { return true; }
    virtual qint64 bytesAvailable() const override
    {
        return active_chunks_.size() * kBufferChunkSize - front_pos_ + back_pos_ - kBufferChunkSize;
    }
    virtual qint64 size() const override
    {
        return bytesAvailable();
    }
protected:
    virtual qint64 readData(char *data, qint64 max_size) override
    {
        qint64 size_read = 0;
        while (active_chunks_.size() > 1)
        {
            qint64 front_size = (qint64)(kBufferChunkSize - front_pos_);
            if (front_size <= max_size)
            {
                memcpy(data, active_chunks_.front().sample_data + front_pos_, (size_t)front_size);
                size_read += front_size;
                data += front_size;
                max_size -= front_size;
                free_chunks_.splice(free_chunks_.end(), active_chunks_, active_chunks_.begin());
                front_pos_ = 0;
            }
            else
            {
                memcpy(data, active_chunks_.front().sample_data + front_pos_, (size_t)max_size);
                size_read += max_size;
                //Don't need to update data and max_size since this is last memcpy
                front_pos_ += (size_t)max_size;
                return size_read;
            }
        }
        if (!active_chunks_.empty())
        {
            qint64 front_size = (qint64)(back_pos_ - front_pos_);
            if (front_size <= max_size)
            {
                memcpy(data, active_chunks_.front().sample_data + front_pos_, (size_t)front_size);
                size_read += front_size;
                //Don't need to update data and max_size since it's last memcpy
                free_chunks_.splice(free_chunks_.end(), active_chunks_, active_chunks_.begin());
                front_pos_ = 0;
                back_pos_ = kBufferChunkSize;
            }
            else
            {
                memcpy(data, active_chunks_.front().sample_data + front_pos_, (size_t)max_size);
                size_read += max_size;
                //Don't need to update data and max_size since it's last memcpy
                front_pos_ += (size_t)max_size;
            }
        }
        return size_read;
    }

    virtual qint64 writeData(const char *data, qint64 max_size) override
    {
        const qint64 size_written = max_size; //Will always write all data
        if (back_pos_ != kBufferChunkSize) //back_pos_ == kBufferChunkSize when active_chunks_.empty()
        {
            qint64 back_size = (qint64)(kBufferChunkSize - back_pos_);
            if (back_size >= max_size)
            {
                memcpy(active_chunks_.back().sample_data + back_pos_, data, (size_t)max_size);
                back_pos_ += (size_t)max_size;
                if (size_written > 0)
                    emit readyRead();
                return size_written;
            }
            else
            {
                memcpy(active_chunks_.back().sample_data + back_pos_, data, (size_t)back_size);
                data += back_size;
                max_size -= back_size;
                //Don't need to update back_pos_ here, since back_size < max_size means it must go into main loop where back_pos_ will be updated
            }
        }
        while (true) //Break is handled inside
        {
            if (free_chunks_.empty())
                active_chunks_.emplace_back();
            else
                active_chunks_.splice(active_chunks_.end(), free_chunks_, free_chunks_.begin());
            if (max_size <= kBufferChunkSize)
            {
                memcpy(active_chunks_.back().sample_data, data, (size_t)max_size);
                back_pos_ = (size_t)max_size;
                break;
            }
            else
            {
                memcpy(active_chunks_.back().sample_data, data, kBufferChunkSize);
                data += kBufferChunkSize;
                max_size -= kBufferChunkSize;
            }
        }
        if (size_written > 0)
            emit readyRead();
        return size_written;
    }
private:
    std::list<BufferChunk> active_chunks_, free_chunks_;
    size_t front_pos_ = 0, back_pos_ = kBufferChunkSize;
};

AudioOutput::AudioOutput(QQuickItem *parent)
    :QQuickItem(parent)
{
    buffer_ = new AudioOutputBuffer(this);
    buffer_->open(QIODevice::ReadWrite);

    QAudioDeviceInfo info = QAudioDeviceInfo::defaultOutputDevice();
    auto preferred_format = info.preferredFormat();

    qDebug("preferred: %dch %dbit %dHz", preferred_format.channelCount(), preferred_format.sampleSize(), preferred_format.sampleRate());

    switch (preferred_format.channelCount())
    {
    case 1:
    {
        channel_layout_ = AV_CH_LAYOUT_MONO;
        break;
    }
    case 2:
    {
        channel_layout_ = AV_CH_LAYOUT_STEREO;
        break;
    }
    default:
    {
        if (preferred_format.channelCount() <= 0)
            throw std::runtime_error("Invalid channel count");
        QList<int> supported_channel_count = info.supportedChannelCounts();
        bool support_mono = false, support_stereo = false;
        for (int channel_count : supported_channel_count)
        {
            if (channel_count == 1)
                support_mono = true;
            else if (channel_count == 2)
                support_stereo = true;
        }
        if (support_stereo)
        {
            channel_layout_ = AV_CH_LAYOUT_STEREO;
            preferred_format.setChannelCount(2);
        }
        else if (support_mono)
        {
            channel_layout_ = AV_CH_LAYOUT_MONO;
            preferred_format.setChannelCount(1);
        }
        else
            throw std::runtime_error("No supported channel layout");
        break;
    }
    }

    unsigned int sample_size_flag = GetSampleSizeSupportFlag(preferred_format.sampleSize());
    unsigned int sample_type_flag = GetSampleTypeSupportFlag(preferred_format.sampleType());
    sample_size_ = preferred_format.bytesPerFrame();
    sample_format_ = GetAVSampleFormat(sample_size_flag, sample_type_flag);
    if (sample_format_ == AV_SAMPLE_FMT_NONE)
    {
        sample_size_flag = sample_type_flag = 0;
        for (int sample_size : info.supportedSampleSizes())
            sample_size_flag |= GetSampleSizeSupportFlag(sample_size);
        for (auto sample_type : info.supportedSampleTypes())
            sample_type_flag |= GetSampleTypeSupportFlag(sample_type);
        sample_format_ = GetAVSampleFormat(sample_size_flag, sample_type_flag);
        switch (sample_format_)
        {
        case AV_SAMPLE_FMT_U8:
            sample_size_ = 1;
            preferred_format.setSampleSize(8);
            preferred_format.setSampleType(QAudioFormat::UnSignedInt);
            break;
        case AV_SAMPLE_FMT_S16:
            sample_size_ = 2;
            preferred_format.setSampleSize(16);
            preferred_format.setSampleType(QAudioFormat::SignedInt);
            break;
        case AV_SAMPLE_FMT_S32:
            sample_size_ = 4;
            preferred_format.setSampleSize(32);
            preferred_format.setSampleType(QAudioFormat::SignedInt);
            break;
        case AV_SAMPLE_FMT_S64:
            sample_size_ = 8;
            preferred_format.setSampleSize(64);
            preferred_format.setSampleType(QAudioFormat::SignedInt);
            break;
        case AV_SAMPLE_FMT_FLT:
            sample_size_ = 4;
            preferred_format.setSampleSize(32);
            preferred_format.setSampleType(QAudioFormat::Float);
            break;
        case AV_SAMPLE_FMT_DBL:
            sample_size_ = 8;
            preferred_format.setSampleSize(64);
            preferred_format.setSampleType(QAudioFormat::Float);
            break;
        default:
            throw std::runtime_error("No supported combination of sample size and sample format");
        }
    }

    sample_rate_ = preferred_format.sampleRate();
    //sample_endian_ = preferred_format.byteOrder();
    preferred_format.setCodec("audio/pcm");
    preferred_format.setByteOrder(QAudioFormat::LittleEndian);

    Q_ASSERT(info.isFormatSupported(preferred_format));
    output_ = new QAudioOutput(preferred_format, this);
    output_->start(buffer_);
}

void AudioOutput::onNewAudioSource(uintptr_t source_id, const AVCodecContext *context)
{
    AudioSource &source = sources_.emplace(source_id, AudioSource()).first->second;
    source.id = source_id;
    source.sample_position = 0;
    source.swr_context = swr_alloc_set_opts(nullptr,
                                            channel_layout_,         sample_format_,      sample_rate_,
                                            context->channel_layout, context->sample_fmt, context->sample_rate,
                                            0, nullptr);
    swr_init(source.swr_context.Get());
}

void AudioOutput::onDeleteAudioSource(uintptr_t source_id)
{
    sources_.erase(source_id);
}

void AudioOutput::onNewAudioFrame(uintptr_t source_id, QSharedPointer<AudioFrame> audio_frame)
{
    //TODO: mix audio
    //TODO: synchronize
    auto itr = sources_.find(source_id);
    if (itr == sources_.end())
        return;
    AudioSource &source = itr->second;

    thread_local std::vector<uint8_t> buffer;
    int in_size = audio_frame->frame->nb_samples;
    int out_size = swr_get_out_samples(source.swr_context.Get(), in_size);
    buffer.resize(out_size * sample_size_);
    uint8_t *out[1] = { buffer.data() };
    int sample_converted = swr_convert(source.swr_context.Get(), out, out_size, (const uint8_t **)audio_frame->frame->data, in_size);
    buffer_->write((char *)buffer.data(), sample_converted * sample_size_);
}
