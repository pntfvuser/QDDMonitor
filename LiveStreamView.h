#ifndef LIVESTREAMVIEW_H
#define LIVESTREAMVIEW_H

#include "VideoFrame.h"

class LiveStreamSource;
class VideoFrameTextureNode;

class LiveStreamView : public QQuickItem
{
    Q_OBJECT
    QML_ELEMENT

    Q_PROPERTY(LiveStreamSource* source READ source WRITE setSource NOTIFY sourceChanged)
public:
    explicit LiveStreamView(QQuickItem *parent = nullptr);

    LiveStreamSource *source() { return current_source_; }
    void setSource(LiveStreamSource *source);
protected:
    QSGNode *updatePaintNode(QSGNode *, UpdatePaintNodeData *) override;
    void geometryChanged(const QRectF &newGeometry, const QRectF &oldGeometry) override;
signals:
    void sourceChanged();
public slots:
    void onNewMedia();
    void onNewFrame(QSharedPointer<VideoFrame> video_frame);

    void debugRefreshSlot();
private:
    void releaseResources() override;

    LiveStreamSource *current_source_ = nullptr;
    QVector<QSharedPointer<VideoFrame>> next_frames_;
};

#endif // LIVESTREAMVIEW_H
