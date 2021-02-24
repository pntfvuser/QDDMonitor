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

    int roomDisplayId() const { return room_display_id_; }
    void setRoomDisplayId(int room_display_id) { room_display_id_ = room_display_id; }

    Q_INVOKABLE void start();
signals:
    void debugStartSignal();
    void infoUpdated(int status, QList<QString> quality_names);
public slots:
    void onRequestUpdateInfo();
    void onRequestActivate(const QString &quality_name);
    void onRequestActivateQn(int qn);
    void onRequestDeactivate();
private slots:
    void OnRequestUpdateInfoProgress();
    void OnRequestUpdateInfoComplete();
    void OnRequestStreamInfoProgress();
    void OnRequestStreamInfoComplete();
    void OnAVStreamProgress();
    void OnAVStreamPush();
    void OnAVStreamComplete();
private:
    int room_display_id_ = -1, room_id_ = -1;
    QHash<QString, int> quality_;

    bool active_ = false;
    QNetworkAccessManager *network_manager_ = nullptr;
    QNetworkReply *info_reply_ = nullptr, *stream_info_reply_ = nullptr, *av_reply_ = nullptr;
    QTimer *push_timer_ = nullptr;
};

#endif // LIVESTREAMSOURCEBILIBILI_H
