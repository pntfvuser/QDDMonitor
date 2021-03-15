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

    virtual QString SourceType() const = 0;
    virtual QJsonObject ToJson() const = 0;
signals:
    void infoUpdated(int status, const QString &description, const QList<QString> &options);

    void invalidSourceArgument();
    void activated();
    void newSubtitleFrame(const QSharedPointer<SubtitleFrame> &audio_frame);
    void deactivated();

    void newInputStream(const QString &url_hint, const QString &record_path);
    void deleteInputStream();
    void setDefaultMediaRecordFile(const QString &file_path);
    void setOneshotMediaRecordFile(const QString &file_path);
public slots:
    void onRequestUpdateInfo();
    void onRequestActivate(const QString &option);
    void onRequestDeactivate();
    void onRequestSetRecordPath(const QString &path);
private slots:
    void OnInvalidMediaRedirector();
    void OnDeleteMediaRedirector();
protected:
    void BeginData();
    size_t PushData(const char *data, size_t size);
    void PushData(QIODevice *device);
    void EndData();
    void CloseData();

    const QString &RecordPath() const { return record_path_; }
private:
    virtual void UpdateInfo() = 0;
    virtual void Activate(const QString &option) = 0;
    virtual void Deactivate() = 0;
    virtual void UpdateRecordPath() {}
    virtual void OnInvalidMedia() {}
    virtual void OnDeleteMedia() {}

    QThread decoder_thread_;
    LiveStreamDecoder *decoder_ = nullptr;

    QString record_path_;
};

#endif // LIVESTREAMSOURCE_H
