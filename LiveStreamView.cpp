#include "pch.h"
#include "LiveStreamView.h"

#include "LiveStreamSource.h"
#include "LiveStreamDecoder.h"
#include "VideoFrameRenderNodeOGL.h"
#include "AudioOutput.h"
#include "LiveStreamSubtitleOverlay.h"

LiveStreamView::LiveStreamView(QQuickItem *parent)
    :QQuickItem(parent)
{
    setFlag(ItemHasContents, true);
    connect(this, &QQuickItem::widthChanged, this, &LiveStreamView::OnWidthChanged);
    connect(this, &QQuickItem::heightChanged, this, &LiveStreamView::OnHeightChanged);

    subtitle_out_ = new LiveStreamSubtitleOverlay(this);
    subtitle_out_->setPosition(QPointF(0, 0));
    subtitle_out_->setSize(size());
}

LiveStreamView::~LiveStreamView()
{
    emit deleteAudioSource(this);
}

void LiveStreamView::setSource(LiveStreamSource *source)
{
    if (source != current_source_)
    {
        if (current_source_)
        {
            disconnect(current_source_->decoder(), &LiveStreamDecoder::newMedia, this, &LiveStreamView::onNewMedia);
            disconnect(current_source_->decoder(), &LiveStreamDecoder::newVideoFrame, this, &LiveStreamView::onNewVideoFrame);
            disconnect(current_source_->decoder(), &LiveStreamDecoder::newAudioFrame, this, &LiveStreamView::onNewAudioFrame);
            disconnect(current_source_, &LiveStreamSource::newSubtitleFrame, this, &LiveStreamView::onNewSubtitleFrame);
        }
        current_source_ = source;
        if (current_source_)
        {
            emit stopAudioSource(this); //Force resynchronize audio and video
            connect(current_source_->decoder(), &LiveStreamDecoder::newMedia, this, &LiveStreamView::onNewMedia);
            connect(current_source_->decoder(), &LiveStreamDecoder::newVideoFrame, this, &LiveStreamView::onNewVideoFrame);
            connect(current_source_->decoder(), &LiveStreamDecoder::newAudioFrame, this, &LiveStreamView::onNewAudioFrame);
            connect(current_source_, &LiveStreamSource::newSubtitleFrame, this, &LiveStreamView::onNewSubtitleFrame);
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
            emit deleteAudioSource(this);
            disconnect(this, &LiveStreamView::newAudioSource, audio_out_, &AudioOutput::onNewAudioSource);
            disconnect(this, &LiveStreamView::stopAudioSource, audio_out_, &AudioOutput::onStopAudioSource);
            disconnect(this, &LiveStreamView::deleteAudioSource, audio_out_, &AudioOutput::onDeleteAudioSource);
            disconnect(this, &LiveStreamView::newAudioFrame, audio_out_, &AudioOutput::onNewAudioFrame);
            disconnect(this, &LiveStreamView::setAudioSourcePosition, audio_out_, &AudioOutput::onSetAudioSourcePosition);
        }
        audio_out_ = audio_out;
        if (audio_out_)
        {
            connect(this, &LiveStreamView::newAudioSource, audio_out_, &AudioOutput::onNewAudioSource);
            connect(this, &LiveStreamView::stopAudioSource, audio_out_, &AudioOutput::onStopAudioSource);
            connect(this, &LiveStreamView::deleteAudioSource, audio_out_, &AudioOutput::onDeleteAudioSource);
            connect(this, &LiveStreamView::newAudioFrame, audio_out_, &AudioOutput::onNewAudioFrame);
            connect(this, &LiveStreamView::setAudioSourceVolume, audio_out_, &AudioOutput::onSetAudioSourceVolume);
            connect(this, &LiveStreamView::setAudioSourcePosition, audio_out_, &AudioOutput::onSetAudioSourcePosition);
        }
        emit audioOutChanged();
    }
}

void LiveStreamView::setVolume(qreal new_volume)
{
    if (volume_ != new_volume)
    {
        volume_ = new_volume;
        emit setAudioSourceVolume(this, new_volume);
        emit volumeChanged();
    }
}

void LiveStreamView::setPosition(const QVector3D &new_position)
{
    if (position_ != new_position)
    {
        position_ = new_position;
        emit setAudioSourcePosition(this, new_position);
        emit positionChanged();
    }
}

void LiveStreamView::setT(qreal new_t)
{
    if (t_ != new_t)
    {
        t_ = new_t;
        update();
        subtitle_out_->setT(t_);
        emit tChanged();
    }
}

QSGNode *LiveStreamView::updatePaintNode(QSGNode *node_base, QQuickItem::UpdatePaintNodeData *)
{
    VideoFrameRenderNodeOGL *node = static_cast<VideoFrameRenderNodeOGL *>(node_base);

    if (!node)
    {
        if (width() <= 0 || height() <= 0)
            return nullptr;
        node = new VideoFrameRenderNodeOGL;
    }

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

void LiveStreamView::onNewVideoFrame(const QSharedPointer<VideoFrame> &video_frame)
{
    if (!current_source_ || sender() != current_source_->decoder())
        return;
    bool need_update = next_frames_.empty();
    if (next_frames_.size() >= 12)
        next_frames_.erase(next_frames_.begin());
    next_frames_.push_back(std::move(video_frame));
    if (need_update)
        update();
}

void LiveStreamView::onNewAudioFrame(const QSharedPointer<AudioFrame> &audio_frame)
{
    if (!current_source_ || sender() != current_source_->decoder())
        return;
    emit newAudioFrame(this, audio_frame);
}

void LiveStreamView::onNewSubtitleFrame(const QSharedPointer<SubtitleFrame> &subtitle_frame)
{
    if (!current_source_ || sender() != current_source_)
        return;
    subtitle_out_->onNewSubtitleFrame(subtitle_frame);
}

void LiveStreamView::OnWidthChanged()
{
    subtitle_out_->setWidth(width());
}

void LiveStreamView::OnHeightChanged()
{
    subtitle_out_->setHeight(width());
}
