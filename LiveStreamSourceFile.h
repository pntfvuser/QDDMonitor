#ifndef LIVESTREAMSOURCEFILE_H
#define LIVESTREAMSOURCEFILE_H

#include "LiveStreamSource.h"

class LiveStreamSourceFile : public LiveStreamSource
{
    Q_OBJECT

    Q_PROPERTY(QString filePath READ filePath WRITE setFilePath NOTIFY filePathChanged)
public:
    explicit LiveStreamSourceFile(QObject *parent = nullptr);
    ~LiveStreamSourceFile();

    QString filePath() const;
    void setFilePath(const QString &file_path);

    Q_INVOKABLE void start();
signals:
    void filePathChanged();

    void newInputStream(void *opaque, SourceInputCallback read_callback);
private slots:
    void DoStart();
    void FeedTick();
private:
    static int AVIOReadCallback(void *opaque, uint8_t *buf, int buf_size);

    QString file_path_;
    QFile *fin_ = nullptr;
    QTimer *feed_timer_ = nullptr;
};

#endif // LIVESTREAMSOURCEFILE_H
