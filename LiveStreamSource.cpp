#include "pch.h"
#include "LiveStreamSource.h"

#include "LiveStreamDecoder.h"

LiveStreamSource::LiveStreamSource(QObject *parent)
    :QObject(parent)
{
    decoder_ = new LiveStreamDecoder;
    decoder_->moveToThread(&decoder_thread_);
    connect(this, &LiveStreamSource::newInputStream, decoder_, &LiveStreamDecoder::onNewInputStream);
    connect(this, &LiveStreamSource::deleteInputStream, decoder_, &LiveStreamDecoder::onDeleteInputStream);
    connect(decoder_, &LiveStreamDecoder::invalidMedia, this, &LiveStreamSource::OnInvalidMedia);
    connect(decoder_, &LiveStreamDecoder::deleteMedia, this, &LiveStreamSource::OnDeleteMedia);
    connect(&decoder_thread_, &QThread::finished, decoder_, &QObject::deleteLater);
    decoder_thread_.start();
}

LiveStreamSource::~LiveStreamSource()
{
    decoder_->CloseData();
    emit deleteInputStream();
    decoder_thread_.exit();
    decoder_thread_.wait();
}

void LiveStreamSource::onRequestUpdateInfo()
{
    updateInfo();
}

void LiveStreamSource::onRequestActivate(const QString &option)
{
    activate(option);
}

void LiveStreamSource::onRequestDeactivate()
{
    deactivate();
}

void LiveStreamSource::OnInvalidMedia()
{
    emit invalidMedia();
}

void LiveStreamSource::OnDeleteMedia()
{
    emit deleteMedia();
}

void LiveStreamSource::BeginData()
{
    return decoder_->BeginData();
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

void LiveStreamSource::CloseData()
{
    return decoder_->CloseData();
}
