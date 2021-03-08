#include "pch.h"
#include "LiveStreamSourceBilibili.h"

#include "LiveStreamSourceBilibiliDanmu.h"

LiveStreamSourceBilibili::LiveStreamSourceBilibili(int room_display_id, QNetworkAccessManager *network_manager, QObject *parent)
    :LiveStreamSource(parent),
    room_display_id_(room_display_id),
    network_manager_(network_manager), av_network_manager_(new QNetworkAccessManager(this)),
    danmu_source_(new LiveStreamSourceBilibiliDanmu(network_manager_, this)),
    push_timer_(new QTimer(this))
{
    connect(push_timer_, &QTimer::timeout, this, &LiveStreamSourceBilibili::OnAVStreamPush);
    push_timer_->setInterval(50);
    description_ = tr("bilibili live room %1").arg(room_display_id);
}

LiveStreamSourceBilibili::~LiveStreamSourceBilibili()
{
}

void LiveStreamSourceBilibili::OnNewDanmu(const QSharedPointer<SubtitleFrame> &danmu_frame)
{
    emit newSubtitleFrame(danmu_frame);
}

void LiveStreamSourceBilibili::UpdateInfo()
{
    if (info_reply_)
        return; //Prevent duplicate request

    QUrl request_url("https://api.live.bilibili.com/xlive/web-room/v2/index/getRoomPlayInfo");
    QUrlQuery request_query;
    request_query.addQueryItem("room_id", QString::number(room_display_id_));
    request_query.addQueryItem("protocol", "0");
    request_query.addQueryItem("format", "0");
    request_query.addQueryItem("codec", "1");
    request_query.addQueryItem("qn", "10000");
    request_query.addQueryItem("platform", "web");
    request_query.addQueryItem("ptype", "16");
    request_url.setQuery(request_query);

    QNetworkRequest request(request_url);
    info_reply_ = network_manager_->get(request);
    connect(info_reply_, &QNetworkReply::readyRead, this, &LiveStreamSourceBilibili::OnRequestUpdateInfoProgress);
    connect(info_reply_, &QNetworkReply::finished, this, &LiveStreamSourceBilibili::OnRequestUpdateInfoComplete);
}

void LiveStreamSourceBilibili::OnRequestUpdateInfoProgress()
{
    if (!info_reply_)
        return;
    if (info_reply_->bytesAvailable() > 2048)
    {
        info_reply_->deleteLater();
        info_reply_ = nullptr;
        emit infoUpdated(-1, description_, QList<QString>());
        return;
    }
}

void LiveStreamSourceBilibili::OnRequestUpdateInfoComplete()
{
    if (!info_reply_)
        return;
    QJsonDocument json_doc = QJsonDocument::fromJson(info_reply_->readAll());
    info_reply_->deleteLater();
    info_reply_ = nullptr;

    if (!json_doc.isObject())
    {
        emit infoUpdated(-1, description_, QList<QString>());
        return;
    }
    QJsonObject json_object = json_doc.object();
    int code = (int)json_object.value("code").toDouble(-1);
    if (code != 0)
    {
        emit infoUpdated(-abs(code), description_, QList<QString>());
        return;
    }
    QJsonValue data_value = json_object.value("data");
    if (!data_value.isObject())
    {
        emit infoUpdated(-1, description_, QList<QString>());
        return;
    }
    QJsonObject data_object = data_value.toObject();

    QJsonValue room_id_value = data_object.value("room_id");
    if (!room_id_value.isDouble())
    {
        emit infoUpdated(-1, description_, QList<QString>());
        return;
    }
    QJsonValue live_status_value = data_object.value("live_status");
    if (!room_id_value.isDouble())
    {
        emit infoUpdated(-1, description_, QList<QString>());
        return;
    }
    room_id_ = (int)room_id_value.toDouble();
    int status = (int)live_status_value.toDouble();
    status = status == 1 ? STATUS_ONLINE : STATUS_OFFLINE;
    quality_.clear();

    QJsonValue qn_desc_value = data_object.value("playurl_info").toObject().value("playurl").toObject().value("g_qn_desc");
    if (!qn_desc_value.isArray())
    {
        emit infoUpdated(status, description_, QList<QString>());
        return;
    }
    QJsonArray qn_desc = qn_desc_value.toArray();
    QList<QString> descs;
    for (const QJsonValue value : qn_desc)
    {
        if (value.isObject())
        {
            QJsonObject value_object = value.toObject();
            QJsonValue qn_value = value_object.value("qn"), desc_value = value_object.value("desc");
            if (qn_value.isDouble() && desc_value.isString())
            {
                QString desc = desc_value.toString();
                quality_.insert(desc, (int)qn_value.toDouble());
                descs.append(desc);
            }
        }
    }
    emit infoUpdated(status, description_, descs);
}

void LiveStreamSourceBilibili::Activate(const QString &quality_name)
{
    if (stream_info_reply_ || av_reply_) //Activating or active
        return;

    if (active_) //Deactivating
    {
        if (quality_name.isEmpty())
        {
            emit invalidSourceArgument();
            return;
        }
        pending_option_ = quality_name;
        return;
    }

    int quality_chosen = quality_.value(quality_name, -1);
    if (quality_chosen == -1)
    {
        emit invalidSourceArgument();
        return;
    }

    if (room_id_ == -1)
    {
        emit invalidSourceArgument();
        return;
    }

    QUrl request_url("https://api.live.bilibili.com/xlive/web-room/v1/playUrl/playUrl");
    QUrlQuery request_query;
    request_query.addQueryItem("cid", QString::number(room_id_));
    request_query.addQueryItem("platform", "web");
    request_query.addQueryItem("qn", QString::number(quality_chosen));
    request_query.addQueryItem("https_url_req", "1");
    request_query.addQueryItem("ptype", "16");
    request_url.setQuery(request_query);

    QNetworkRequest request(request_url);
    stream_info_reply_ = network_manager_->get(request);
    if (!stream_info_reply_)
    {
        emit invalidSourceArgument();
        return;
    }
    connect(stream_info_reply_, &QNetworkReply::readyRead, this, &LiveStreamSourceBilibili::OnRequestStreamInfoProgress);
    connect(stream_info_reply_, &QNetworkReply::finished, this, &LiveStreamSourceBilibili::OnRequestStreamInfoComplete);
}

void LiveStreamSourceBilibili::OnRequestStreamInfoProgress()
{
    if (!stream_info_reply_ || sender() != stream_info_reply_)
        return;
    if (stream_info_reply_->bytesAvailable() > 2048)
    {
        stream_info_reply_->deleteLater();
        stream_info_reply_ = nullptr;
        emit invalidSourceArgument();
        return;
    }
}

void LiveStreamSourceBilibili::OnRequestStreamInfoComplete()
{
    if (!stream_info_reply_ || sender() != stream_info_reply_)
        return;
    QJsonDocument json_doc = QJsonDocument::fromJson(stream_info_reply_->readAll());
    stream_info_reply_->deleteLater();
    stream_info_reply_ = nullptr;

    Q_ASSERT(!active_);
    do
    {
        if (!json_doc.isObject())
            break;
        QJsonObject json_object = json_doc.object();
        int code = (int)json_object.value("code").toDouble(-1);
        if (code != 0)
            break;
        QJsonValue data_value = json_object.value("data");
        if (!data_value.isObject())
            break;
        QJsonObject data_object = data_value.toObject();

        QJsonValue durl_value = data_object.value("durl");
        if (!durl_value.isArray())
            break;
        QJsonArray durl = durl_value.toArray();
        if (durl.empty())
            break;

        QJsonValue first_url = durl[0].toObject().value("url");
        if (!first_url.isString())
            break;

        QUrl request_url(first_url.toString());
        QNetworkRequest request(request_url);
        request.setAttribute(QNetworkRequest::RedirectPolicyAttribute, QNetworkRequest::NoLessSafeRedirectPolicy);
        request.setMaximumRedirectsAllowed(2);
        request.setRawHeader("User-Agent", "Mozilla/5.0 (Windows NT 10.0; Win64; x64) AppleWebKit/537.36 (KHTML, like Gecko) Chrome/88.0.4324.190 Safari/537.36");
        request.setRawHeader("Origin", "https://live.bilibili.com");
        request.setRawHeader("Referer", "https://live.bilibili.com/");
        av_reply_ = av_network_manager_->get(request);
        if (!av_reply_)
            break;
        active_ = true;
        emit activated();

        BeginData();
        emit newInputStream("stream.flv");
        connect(av_reply_, &QNetworkReply::readyRead, this, &LiveStreamSourceBilibili::OnAVStreamProgress);
        push_timer_->start();

        danmu_source_->Activate(room_id_, 3);
    } while (false);
    if (!active_)
    {
        emit invalidSourceArgument();
        return;
    }
}

void LiveStreamSourceBilibili::OnAVStreamProgress()
{
    if (!av_reply_ || sender() != av_reply_)
        return;

    PushData(av_reply_);
    if (av_reply_->isFinished() && av_reply_->bytesAvailable() <= 0)
    {
        qDebug() << av_reply_->error() << ' ' << av_reply_->errorString();
        push_timer_->stop();
        EndData();
    }
}

void LiveStreamSourceBilibili::OnAVStreamPush()
{
    if (!av_reply_)
    {
        push_timer_->stop();
        return;
    }
    if (av_reply_->bytesAvailable() > 0)
    {
        PushData(av_reply_);
    }
    if (av_reply_->isFinished() && av_reply_->bytesAvailable() <= 0)
    {
        qDebug() << av_reply_->error() << ' ' << av_reply_->errorString();
        push_timer_->stop();
        EndData();
    }
}

void LiveStreamSourceBilibili::Deactivate()
{
    if (av_reply_)
    {
        push_timer_->stop();
        av_reply_->close();
        av_reply_->deleteLater();
        av_reply_ = nullptr;
        CloseData();
        emit deleteInputStream();
    }
}

void LiveStreamSourceBilibili::OnInvalidMedia()
{
    OnDeleteMedia();
}

void LiveStreamSourceBilibili::OnDeleteMedia()
{
    if (!active_)
        return;
    push_timer_->stop();
    if (av_reply_)
    {
        av_reply_->deleteLater();
        av_reply_ = nullptr;
        //No need to CloseData since LiveStreamDecoder::Close() has already done that
    }
    danmu_source_->Deactivate();
    active_ = false;
    emit deactivated();

    if (!pending_option_.isEmpty())
    {
        QTimer::singleShot(0, this, [this]()
        {
            if (!pending_option_.isEmpty())
            {
                QString option = pending_option_;
                pending_option_.clear();
                Activate(option);
            }
        });
    }
}
