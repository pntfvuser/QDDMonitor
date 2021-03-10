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

    static LiveStreamSourceFile *FromJson(const QJsonObject &json, QObject *parent = nullptr);
    virtual QString SourceType() const override;
    virtual QJsonObject ToJson() const override;
signals:
    void filePathChanged();
private:
    virtual void UpdateInfo() override;
    virtual void Activate(const QString &option) override;
    virtual void Deactivate() override;

    void FeedTick();

    QString file_path_;
    QFile *fin_ = nullptr;
    QTimer *feed_timer_ = nullptr;
};

#endif // LIVESTREAMSOURCEFILE_H
