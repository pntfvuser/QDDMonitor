#ifndef LIVESTREAMSOURCE_H
#define LIVESTREAMSOURCE_H

#include "SubtitleFrame.h"

class LiveStreamSourceDecoder;

class LiveStreamSource : public QObject
{
    Q_OBJECT

public:
    explicit LiveStreamSource(QObject *parent = nullptr);
    ~LiveStreamSource();

    bool open() const { return open_; }
    LiveStreamSourceDecoder *decoder() { return decoder_; }
signals:
    void openChanged(bool new_open);

    void RequestNewInputStream(QString url_hint);
    void RequestDeleteInputStream();

    void InvalidSourceArgument();
    void NewSubtitleFrame(QSharedPointer<SubtitleFrame> audio_frame);
private slots:
    void OnInvalidMedia();
    void OnNewMedia(const AVCodecContext *video_decoder_context, const AVCodecContext *audio_decoder_context);
    void OnDeleteMedia();
protected:
    size_t PushData(const char *data, size_t size);
    void PushData(QIODevice *device);
    void EndData();
private:
    QThread decoder_thread_;
    LiveStreamSourceDecoder *decoder_ = nullptr;
    bool open_ = false;
};

#endif // LIVESTREAMSOURCE_H
