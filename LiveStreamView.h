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
    Q_PROPERTY(int t READ t WRITE setT NOTIFY tChanged)
public:
    explicit LiveStreamView(QQuickItem *parent = nullptr);

    LiveStreamSource *source() { return current_source_; }
    void setSource(LiveStreamSource *source);

    int t() const { return t_; }
    void setT(int new_t) { if (t_ != new_t) { t_ = new_t; emit tChanged(); } }
protected:
    QSGNode *updatePaintNode(QSGNode *, UpdatePaintNodeData *) override;
    void geometryChanged(const QRectF &newGeometry, const QRectF &oldGeometry) override;
signals:
    void sourceChanged();

    void tChanged();
public slots:
    void onNewMedia();
    void onNewVideoFrame(QSharedPointer<VideoFrame> video_frame);
private slots:
    void onTChanged();
private:
    void releaseResources() override;

    int t_ = 0;
    LiveStreamSource *current_source_ = nullptr;
    QVector<QSharedPointer<VideoFrame>> next_frames_;
};

#endif // LIVESTREAMVIEW_H
