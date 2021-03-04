#include "pch.h"
#include "LiveStreamView.h"

#include "LiveStreamSource.h"
#include "LiveStreamDecoder.h"
#include "VideoFrameTextureNode.h"
#include "AudioOutput.h"
#include "LiveStreamSubtitleOverlay.h"

LiveStreamView::LiveStreamView(QQuickItem *parent)
    :QQuickItem(parent)
{
    setFlag(ItemHasContents, true);
    connect(this, &QQuickItem::widthChanged, this, &LiveStreamView::OnWidthChanged);
    connect(this, &QQuickItem::heightChanged, this, &LiveStreamView::OnHeightChanged);
    connect(this, &LiveStreamView::tChanged, this, &LiveStreamView::OnTChanged);

    subtitle_out_ = new LiveStreamSubtitleOverlay(this);
    subtitle_out_->setPosition(QPointF(0, 0));
    subtitle_out_->setSize(size());
}

LiveStreamView::~LiveStreamView()
{
    if (view_index_ != -1)
        emit deleteAudioSource(view_index_);
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
            if (view_index_ != -1)
                emit deleteAudioSource(view_index_); //Force resynchronize audio and video
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
            if (view_index_ != -1)
                emit deleteAudioSource(view_index_);
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

void LiveStreamView::setViewIndex(int new_view_index)
{
    if (new_view_index >= -1 && view_index_ != new_view_index)
    {
        if (view_index_ != -1)
            emit deleteAudioSource(view_index_);
        view_index_ = new_view_index;
        emit viewIndexChanged();
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

void LiveStreamView::onNewVideoFrame(const QSharedPointer<VideoFrame> &video_frame)
{
    if (!current_source_ || sender() != current_source_->decoder())
        return;
    bool need_update = next_frames_.empty();
    next_frames_.push_back(std::move(video_frame));
    if (need_update)
        update();
}

void LiveStreamView::onNewAudioFrame(const QSharedPointer<AudioFrame> &audio_frame)
{
    if (!current_source_ || sender() != current_source_->decoder())
        return;
    if (view_index_ == -1)
        return;
    emit newAudioFrame(view_index_, audio_frame);
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

void LiveStreamView::OnTChanged()
{
    update();
    subtitle_out_->setT(t_);
}
