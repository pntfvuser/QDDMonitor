#include "pch.h"
#include "LiveStreamView.h"

#include "VideoFrameTextureNode.h"
#include "LiveStreamSource.h"

LiveStreamView::LiveStreamView(QQuickItem *parent)
    :QQuickItem(parent)
{
    setFlag(ItemHasContents, true);
}

void LiveStreamView::setSource(LiveStreamSource *source)
{
    if (source != current_source_)
    {
        if (current_source_)
        {
            disconnect(current_source_, &LiveStreamSource::newMedia, this, &LiveStreamView::onNewMedia);
            disconnect(current_source_, &LiveStreamSource::newVideoFrame, this, &LiveStreamView::onNewFrame);
            disconnect(current_source_, &LiveStreamSource::temporaryRefreshSignal, this, &LiveStreamView::temporaryRefreshSlot);
        }
        current_source_ = source;
        connect(current_source_, &LiveStreamSource::newMedia, this, &LiveStreamView::onNewMedia);
        connect(current_source_, &LiveStreamSource::newVideoFrame, this, &LiveStreamView::onNewFrame);
        connect(current_source_, &LiveStreamSource::temporaryRefreshSignal, this, &LiveStreamView::temporaryRefreshSlot);
        emit sourceChanged();
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
        for (const auto &p : next_frames_)
            node->AddVideoFrame(p);
        next_frames_.clear();
    }

    node->Synchronize();

    return node;
}

void LiveStreamView::geometryChanged(const QRectF &newGeometry, const QRectF &oldGeometry)
{
    QQuickItem::geometryChanged(newGeometry, oldGeometry);

    if (newGeometry.size() != oldGeometry.size())
        update();
}

void LiveStreamView::onNewMedia()
{
}

void LiveStreamView::onNewFrame(QSharedPointer<VideoFrame> video_frame)
{
    bool need_update = next_frames_.empty();
    next_frames_.push_back(video_frame);
    if (need_update)
        update();
}

void LiveStreamView::temporaryRefreshSlot()
{
    update();
}

void LiveStreamView::invalidateSceneGraph()
{
    //node_ = nullptr;
}

void LiveStreamView::releaseResources()
{
    //node_ = nullptr;
}
