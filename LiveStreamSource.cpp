#include "pch.h"
#include "LiveStreamSource.h"

#include "LiveStreamSourceDecoder.h"

LiveStreamSource::LiveStreamSource(QObject *parent)
    :QObject(parent)
{
    decoder_ = new LiveStreamSourceDecoder;
    decoder_->moveToThread(&decoder_thread_);
    connect(this, &LiveStreamSource::RequestNewInputStream, decoder_, &LiveStreamSourceDecoder::OnNewInputStream);
    connect(this, &LiveStreamSource::RequestDeleteInputStream, decoder_, &LiveStreamSourceDecoder::OnDeleteInputStream);
    connect(decoder_, &LiveStreamSourceDecoder::InvalidMedia, this, &LiveStreamSource::OnInvalidMedia);
    connect(decoder_, &LiveStreamSourceDecoder::NewMedia, this, &LiveStreamSource::OnNewMedia);
    connect(decoder_, &LiveStreamSourceDecoder::DeleteMedia, this, &LiveStreamSource::OnDeleteMedia);
    connect(&decoder_thread_, &QThread::finished, decoder_, &QObject::deleteLater);
    decoder_thread_.start();
}

LiveStreamSource::~LiveStreamSource()
{
    decoder_->EndData();
    emit RequestDeleteInputStream();
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
