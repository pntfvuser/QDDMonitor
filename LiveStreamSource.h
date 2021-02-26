#ifndef LIVESTREAMSOURCE_H
#define LIVESTREAMSOURCE_H

#include "SubtitleFrame.h"

class LiveStreamDecoder;

class LiveStreamSource : public QObject
{
    Q_OBJECT

public:
    enum StatusCode
    {
        STATUS_OFFLINE = 0,
        STATUS_ONLINE = 1,
    };
    Q_ENUM(StatusCode);

    explicit LiveStreamSource(QObject *parent = nullptr);
    ~LiveStreamSource();

    LiveStreamDecoder *decoder() const { return decoder_; }
signals:
    void infoUpdated(int status, QString description, QList<QString> options);

    void invalidSourceArgument();
    void invalidMedia();
    void newSubtitleFrame(QSharedPointer<SubtitleFrame> audio_frame);
    void deleteMedia();

    void newInputStream(QString url_hint);
    void deleteInputStream();
public slots:
    void onRequestUpdateInfo();
    void onRequestActivate(const QString &option);
    void onRequestDeactivate();
private slots:
    void OnInvalidMedia();
    void OnDeleteMedia();
protected:
    void BeginData();
    size_t PushData(const char *data, size_t size);
    void PushData(QIODevice *device);
    void EndData();
    void CloseData();
private:
    virtual void updateInfo() = 0;
    virtual void activate(const QString &option) = 0;
    virtual void deactivate() = 0;

    QThread decoder_thread_;
    LiveStreamDecoder *decoder_ = nullptr;
};

#endif // LIVESTREAMSOURCE_H
