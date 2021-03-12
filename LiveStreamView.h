#ifndef LIVESTREAMVIEW_H
#define LIVESTREAMVIEW_H

#include "VideoFrame.h"
#include "AudioFrame.h"
#include "SubtitleFrame.h"

class LiveStreamSource;
class AudioOutput;
class LiveStreamSubtitleOverlay;

class LiveStreamView : public QQuickItem
{
    Q_OBJECT

    Q_PROPERTY(LiveStreamSource* source READ source WRITE setSource NOTIFY sourceChanged)
    Q_PROPERTY(AudioOutput* audioOut READ audioOut WRITE setAudioOut NOTIFY audioOutChanged)
    Q_PROPERTY(LiveStreamSubtitleOverlay* subtitleOut READ subtitleOut NOTIFY subtitleOutChanged)

    Q_PROPERTY(qreal volume READ volume WRITE setVolume NOTIFY volumeChanged)
    Q_PROPERTY(QVector3D position READ position WRITE setPosition NOTIFY positionChanged)

    Q_PROPERTY(qreal t READ t WRITE setT NOTIFY tChanged)
public:
    static constexpr int kAnimationTimeSourcePeriod = 10000;

    explicit LiveStreamView(QQuickItem *parent = nullptr);
    ~LiveStreamView();

    LiveStreamSource *source() const { return current_source_; }
    void setSource(LiveStreamSource *source);
    AudioOutput *audioOut() const { return audio_out_; }
    void setAudioOut(AudioOutput *audio_out);
    LiveStreamSubtitleOverlay *subtitleOut() const { return subtitle_out_; }

    qreal volume() const { return volume_; }
    void setVolume(qreal new_volume);
    QVector3D position() const { return position_; }
    void setPosition(const QVector3D &new_position);

    qreal t() const { return t_; }
    void setT(qreal new_t);
protected:
    QSGNode *updatePaintNode(QSGNode *, UpdatePaintNodeData *) override;
    void geometryChanged(const QRectF &newGeometry, const QRectF &oldGeometry) override;
signals:
    void sourceChanged();
    void audioOutChanged();
    void subtitleOutChanged();

    void volumeChanged();
    void positionChanged();

    void newAudioSource(void *source_id, const AVCodecContext *context);
    void stopAudioSource(void *source_id);
    void deleteAudioSource(void *source_id);
    void newAudioFrame(void *source_id, const QSharedPointer<AudioFrame> &audio_frame);
    void setAudioSourceVolume(void *source_id, qreal volume);
    void setAudioSourcePosition(void *source_id, QVector3D position);

    void tChanged();
public slots:
    void onNewMedia(const AVCodecContext *video_decoder_context, const AVCodecContext *audio_decoder_context);
    void onNewVideoFrame(const QSharedPointer<VideoFrame> &video_frame);
    void onNewAudioFrame(const QSharedPointer<AudioFrame> &audio_frame);
    void onNewSubtitleFrame(const QSharedPointer<SubtitleFrame> &subtitle_frame);
private slots:
    void OnWidthChanged();
    void OnHeightChanged();
private:
    LiveStreamSource *current_source_ = nullptr;
    std::vector<QSharedPointer<VideoFrame>> next_frames_;
    AudioOutput *audio_out_ = nullptr;
    LiveStreamSubtitleOverlay *subtitle_out_ = nullptr;

    qreal volume_ = 1;
    QVector3D position_;

    qreal t_ = 0;
};

#endif // LIVESTREAMVIEW_H
