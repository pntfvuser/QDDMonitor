#include "pch.h"
#include "LiveStreamView.h"

#include "LiveStreamSource.h"
#include "VideoFrameTextureNode.h"
#include "AudioOutput.h"

LiveStreamView::LiveStreamView(QQuickItem *parent)
    :QQuickItem(parent)
{
    setFlag(ItemHasContents, true);
    connect(this, &LiveStreamView::tChanged, this, &LiveStreamView::onTChanged);
}

LiveStreamView::~LiveStreamView()
{
    emit deleteAudioSource(reinterpret_cast<uintptr_t>(this));
}

void LiveStreamView::setSource(LiveStreamSource *source)
{
    if (source != current_source_)
    {
        if (current_source_)
        {
            disconnect(current_source_, &LiveStreamSource::newMedia, this, &LiveStreamView::onNewMedia);
            disconnect(current_source_, &LiveStreamSource::newVideoFrame, this, &LiveStreamView::onNewVideoFrame);
            disconnect(current_source_, &LiveStreamSource::newAudioFrame, this, &LiveStreamView::onNewAudioFrame);
        }
        current_source_ = source;
        if (current_source_)
        {
            connect(current_source_, &LiveStreamSource::newMedia, this, &LiveStreamView::onNewMedia);
            connect(current_source_, &LiveStreamSource::newVideoFrame, this, &LiveStreamView::onNewVideoFrame);
            connect(current_source_, &LiveStreamSource::newAudioFrame, this, &LiveStreamView::onNewAudioFrame);
        }
        emit sourceChanged();
    }
}

void LiveStreamView::setAudioOut(AudioOutput *audio_out)
{
    if (audio_out != audio_out_)
    {
        if (audio_out_)
        {
            emit deleteAudioSource(reinterpret_cast<uintptr_t>(this));
            disconnect(this, &LiveStreamView::newAudioSource, audio_out_, &AudioOutput::onNewAudioSource);
            disconnect(this, &LiveStreamView::deleteAudioSource, audio_out_, &AudioOutput::onDeleteAudioSource);
            disconnect(this, &LiveStreamView::newAudioFrame, audio_out_, &AudioOutput::onNewAudioFrame);
        }
        audio_out_ = audio_out;
        if (audio_out_)
        {
            connect(this, &LiveStreamView::newAudioSource, audio_out_, &AudioOutput::onNewAudioSource);
            connect(this, &LiveStreamView::deleteAudioSource, audio_out_, &AudioOutput::onDeleteAudioSource);
            connect(this, &LiveStreamView::newAudioFrame, audio_out_, &AudioOutput::onNewAudioFrame);
        }
        emit audioOutChanged();
    }
}

QSGNode *LiveStreamView::updatePaintNode(QSGNode *node_base, QQuickItem::UpdatePaintNodeData *)
{
    VideoFrameTextureNode *node = static_cast<VideoFrameTextureNode *>(node_base);

    if (!node)
    {
        if (width() <= 0 || height() <= 0)
            return nullptr;
        node = new VideoFrameTextureNode(this);
        node->setTextureCoordinatesTransform(QSGSimpleTextureNode::NoTransform);
        node->setFiltering(QSGTexture::Nearest);
    }

    node->setRect(0, 0, width(), height());
    if (!next_frames_.empty())
    {
        node->AddVideoFrames(std::move(next_frames_));
        next_frames_.clear();
    }

    node->Synchronize(this);

    return node;
}

void LiveStreamView::geometryChanged(const QRectF &newGeometry, const QRectF &oldGeometry)
{
    QQuickItem::geometryChanged(newGeometry, oldGeometry);

    if (newGeometry.size() != oldGeometry.size())
        update();
}

void LiveStreamView::onNewMedia(const AVCodecContext *video_decoder_context, const AVCodecContext *audio_decoder_context)
{
    Q_UNUSED(video_decoder_context);
    Q_UNUSED(audio_decoder_context);
}

void LiveStreamView::onNewVideoFrame(QSharedPointer<VideoFrame> video_frame)
{
    bool need_update = next_frames_.empty();
    next_frames_.push_back(std::move(video_frame));
    if (need_update)
        update();
}

void LiveStreamView::onNewAudioFrame(QSharedPointer<AudioFrame> audio_frame)
{
    emit newAudioFrame(reinterpret_cast<uintptr_t>(this), audio_frame);
}

void LiveStreamView::onTChanged()
{
    update();
}

void LiveStreamView::releaseResources()
{
    //node_ = nullptr;
}
