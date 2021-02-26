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
signals:
    void filePathChanged();
private:
    virtual void updateInfo() override;
    virtual void activate(const QString &option) override;
    virtual void deactivate() override;

    void FeedTick();

    QString file_path_;
    QFile *fin_ = nullptr;
    QTimer *feed_timer_ = nullptr;
};

#endif // LIVESTREAMSOURCEFILE_H
