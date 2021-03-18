#ifndef LIVESTREAMSOURCEBILIBILI_H
#define LIVESTREAMSOURCEBILIBILI_H

#include "LiveStreamSource.h"

class LiveStreamSourceBilibiliDanmu;

class LiveStreamSourceBilibili : public LiveStreamSource
{
    Q_OBJECT

public:
    explicit LiveStreamSourceBilibili(int room_display_id, QNetworkAccessManager *network_manager, QObject *parent = nullptr);
    ~LiveStreamSourceBilibili();

    void OnNewDanmu(const QSharedPointer<SubtitleFrame> &danmu_frame);

    static LiveStreamSourceBilibili *FromJson(const QJsonObject &json, QNetworkAccessManager *network_manager, QObject *parent = nullptr);
    virtual QString SourceType() const override;
    virtual QJsonObject ToJson() const override;
private slots:
    void OnRequestUpdateInfoProgress();
    void OnRequestUpdateInfoComplete();
    void OnRequestRoomInfoProgress();
    void OnRequestRoomInfoComplete();
    void OnRequestStreamInfoProgress();
    void OnRequestStreamInfoComplete();
    void OnAVStreamProgress();
    void OnAVStreamPush();
private:
    virtual void UpdateInfo() override;
    virtual void Activate(const QString &option) override;
    virtual void Deactivate() override;
    virtual void UpdateRecordPath() override;
    virtual void OnInvalidMedia() override;
    virtual void OnDeleteMedia() override;

    QString GenerateRecordFileName() const;

    int room_display_id_ = -1, room_id_ = -1;
    int status_ = STATUS_OFFLINE;
    QList<QString> quality_desc_;
    QHash<QString, int> quality_;

    bool active_ = false;
    QString pending_option_;
    QNetworkAccessManager *network_manager_ = nullptr, *av_network_manager_ = nullptr;
    QNetworkReply *info_reply_ = nullptr, *stream_info_reply_ = nullptr, *av_reply_ = nullptr;
    LiveStreamSourceBilibiliDanmu *danmu_source_ = nullptr;
    QTimer *push_timer_ = nullptr;
};

#endif // LIVESTREAMSOURCEBILIBILI_H
