#ifndef LIVESTREAMSOURCEBILIBILI_H
#define LIVESTREAMSOURCEBILIBILI_H

#include "LiveStreamSource.h"

class LiveStreamSourceBilibili : public LiveStreamSource
{
    Q_OBJECT

public:
    explicit LiveStreamSourceBilibili(QObject *parent = nullptr);
    explicit LiveStreamSourceBilibili(int room_display_id, QObject *parent = nullptr);
    ~LiveStreamSourceBilibili();
private slots:
    void OnRequestUpdateInfoProgress();
    void OnRequestUpdateInfoComplete();
    void OnRequestStreamInfoProgress();
    void OnRequestStreamInfoComplete();
    void OnAVStreamProgress();
    void OnAVStreamPush();
    void OnDeleteMedia();
private:
    virtual void updateInfo() override;
    virtual void activate(const QString &option) override;
    virtual void deactivate() override;

    int room_display_id_ = -1, room_id_ = -1;
    QString description_;
    QHash<QString, int> quality_;

    bool active_ = false;
    QString pending_option_;
    QNetworkAccessManager *network_manager_ = nullptr;
    QNetworkReply *info_reply_ = nullptr, *stream_info_reply_ = nullptr, *av_reply_ = nullptr;
    QTimer *push_timer_ = nullptr;
};

#endif // LIVESTREAMSOURCEBILIBILI_H
