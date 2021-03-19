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
    connect(this, &LiveStreamSource::clearBuffer, decoder_, &LiveStreamDecoder::onClearBuffer);
    connect(this, &LiveStreamSource::setDefaultMediaRecordFile, decoder_, &LiveStreamDecoder::onSetDefaultMediaRecordFile);
    connect(this, &LiveStreamSource::setOneshotMediaRecordFile, decoder_, &LiveStreamDecoder::onSetOneshotMediaRecordFile);
    connect(decoder_, &LiveStreamDecoder::invalidMedia, this, &LiveStreamSource::OnInvalidMediaRedirector);
    connect(decoder_, &LiveStreamDecoder::deleteMedia, this, &LiveStreamSource::OnDeleteMediaRedirector);
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
    UpdateInfo();
}

void LiveStreamSource::onRequestActivate(const QString &option)
{
    Activate(option);
}

void LiveStreamSource::onRequestDeactivate()
{
    Deactivate();
}

void LiveStreamSource::onRequestClearBuffer()
{
    emit clearBuffer();
}

void LiveStreamSource::onRequestSetRecordPath(const QString &path)
{
    record_path_ = path;
    UpdateRecordPath();
}

void LiveStreamSource::OnInvalidMediaRedirector()
{
    OnInvalidMedia();
}

void LiveStreamSource::OnDeleteMediaRedirector()
{
    OnDeleteMedia();
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
