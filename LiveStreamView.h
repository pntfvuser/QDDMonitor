#ifndef LIVESTREAMVIEW_H
#define LIVESTREAMVIEW_H

#include "VideoFrame.h"
#include "AudioFrame.h"
#include "SubtitleFrame.h"

class LiveStreamSource;
class VideoFrameTextureNode;
class AudioOutput;
class LiveStreamSubtitleOverlay;

class LiveStreamView : public QQuickItem
{
    Q_OBJECT

    Q_PROPERTY(LiveStreamSource* source READ source WRITE setSource NOTIFY sourceChanged)
    Q_PROPERTY(AudioOutput* audioOut READ audioOut WRITE setAudioOut NOTIFY audioOutChanged)
    Q_PROPERTY(int viewIndex READ viewIndex WRITE setViewIndex NOTIFY viewIndexChanged)
    Q_PROPERTY(qreal t READ t WRITE setT NOTIFY tChanged)
public:
    static constexpr int kAnimationTimeSourcePeriod = 10000;

    explicit LiveStreamView(QQuickItem *parent = nullptr);
    ~LiveStreamView();

    LiveStreamSource *source() const { return current_source_; }
    void setSource(LiveStreamSource *source);

    AudioOutput *audioOut() const { return audio_out_; }
    void setAudioOut(AudioOutput *audio_out);

    int viewIndex() const { return view_index_; }
    void setViewIndex(int new_view_index);
    qreal t() const { return t_; }
    void setT(qreal new_t) { if (t_ != new_t) { t_ = new_t; emit tChanged(); } }
protected:
    QSGNode *updatePaintNode(QSGNode *, UpdatePaintNodeData *) override;
    void geometryChanged(const QRectF &newGeometry, const QRectF &oldGeometry) override;
signals:
    void sourceChanged();
    void audioOutChanged();

    void newAudioSource(int source_id, const AVCodecContext *context);
    void deleteAudioSource(int source_id);
    void newAudioFrame(int source_id, QSharedPointer<AudioFrame> audio_frame);

    void viewIndexChanged();
    void tChanged();
public slots:
    void onNewMedia(const AVCodecContext *video_decoder_context, const AVCodecContext *audio_decoder_context);
    void onNewVideoFrame(const QSharedPointer<VideoFrame> &video_frame);
    void onNewAudioFrame(const QSharedPointer<AudioFrame> &audio_frame);
    void onNewSubtitleFrame(const QSharedPointer<SubtitleFrame> &subtitle_frame);
private slots:
    void OnWidthChanged();
    void OnHeightChanged();

    void OnTChanged();
private:
    int view_index_ = -1;
    qreal t_ = 0;
    LiveStreamSource *current_source_ = nullptr;
    std::vector<QSharedPointer<VideoFrame>> next_frames_;
    AudioOutput *audio_out_ = nullptr;
    LiveStreamSubtitleOverlay *subtitle_out_ = nullptr;
};

#endif // LIVESTREAMVIEW_H
