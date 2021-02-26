#include "pch.h"
#include "LiveStreamSource.h"

#include "LiveStreamDecoder.h"

LiveStreamSource::LiveStreamSource(QObject *parent)
    :QObject(parent)
{
    decoder_ = new LiveStreamDecoder;
    decoder_->moveToThread(&decoder_thread_);
    connect(this, &LiveStreamSource::requestNewInputStream, decoder_, &LiveStreamDecoder::onNewInputStream);
    connect(this, &LiveStreamSource::requestDeleteInputStream, decoder_, &LiveStreamDecoder::onDeleteInputStream);
    connect(decoder_, &LiveStreamDecoder::invalidMedia, this, &LiveStreamSource::OnInvalidMedia);
    connect(decoder_, &LiveStreamDecoder::newMedia, this, &LiveStreamSource::OnNewMedia);
    connect(decoder_, &LiveStreamDecoder::deleteMedia, this, &LiveStreamSource::OnDeleteMedia);
    connect(&decoder_thread_, &QThread::finished, decoder_, &QObject::deleteLater);
    decoder_thread_.start();
}

LiveStreamSource::~LiveStreamSource()
{
    decoder_->EndData();
    emit requestDeleteInputStream();
    decoder_thread_.exit();
    decoder_thread_.wait();
}

void LiveStreamSource::OnInvalidMedia()
{
    if (open_)
    {
        open_ = false;
        emit openChanged(false);
    }
}

void LiveStreamSource::OnNewMedia(const AVCodecContext *video_decoder_context, const AVCodecContext *audio_decoder_context)
{
    Q_UNUSED(video_decoder_context);
    Q_UNUSED(audio_decoder_context);
    if (!open_)
    {
        open_ = true;
        emit openChanged(true);
    }
}

void LiveStreamSource::OnDeleteMedia()
{
    if (open_)
    {
        open_ = false;
        emit openChanged(false);
    }
}

size_t LiveStreamSource::PushData(const char *data, size_t size)
{
    return decoder_->PushData(data, size);
}

void LiveStreamSource::PushData(QIODevice *source)
{
    return decoder_->PushData(source);
}

void LiveStreamSource::EndData()
{
    return decoder_->EndData();
}
