#include "pch.h"
#include "LiveStreamSourceBilibili.h"

LiveStreamSourceBilibili::LiveStreamSourceBilibili(QObject *parent)
    :LiveStreamSource(parent)
{
    network_manager_ = new QNetworkAccessManager(this);
    connect(this, &LiveStreamSourceBilibili::debugStartSignal, this, &LiveStreamSourceBilibili::onRequestUpdateInfo);
    push_timer_ = new QTimer(this);
    connect(push_timer_, &QTimer::timeout, this, &LiveStreamSourceBilibili::OnAVStreamPush);
}

LiveStreamSourceBilibili::LiveStreamSourceBilibili(int room_display_id, QObject *parent)
    :LiveStreamSource(parent), room_display_id_(room_display_id)
{
    network_manager_ = new QNetworkAccessManager(this);
    connect(this, &LiveStreamSourceBilibili::debugStartSignal, this, &LiveStreamSourceBilibili::onRequestUpdateInfo);
    push_timer_ = new QTimer(this);
    connect(push_timer_, &QTimer::timeout, this, &LiveStreamSourceBilibili::OnAVStreamPush);
}

LiveStreamSourceBilibili::~LiveStreamSourceBilibili()
{
}

void LiveStreamSourceBilibili::start()
{
    emit debugStartSignal();
}

void LiveStreamSourceBilibili::onRequestUpdateInfo()
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

void LiveStreamSourceBilibili::onRequestActivate(const QString &quality_name)
{
    if (stream_info_reply_)
        return;

    int quality_chosen = quality_.value(quality_name, -1);
    if (quality_chosen == -1)
    {
        emit InvalidSourceArgument();
        return;
    }

    onRequestActivateQn(quality_chosen);
}

void LiveStreamSourceBilibili::onRequestActivateQn(int qn)
{
    if (stream_info_reply_)
        return;

    if (room_id_ == -1)
    {
        emit InvalidSourceArgument();
        return;
    }

    if (av_reply_)
    {
        EndData();
        emit RequestDeleteInputStream();
        av_reply_->deleteLater();
        av_reply_ = nullptr;
    }

    QUrl request_url("https://api.live.bilibili.com/xlive/web-room/v1/playUrl/playUrl");
    QUrlQuery request_query;
    request_query.addQueryItem("cid", QString::number(room_id_));
    request_query.addQueryItem("platform", "web");
    request_query.addQueryItem("qn", QString::number(qn));
    request_query.addQueryItem("https_url_req", "1");
    request_query.addQueryItem("ptype", "16");
    request_url.setQuery(request_query);

    QNetworkRequest request(request_url);
    stream_info_reply_ = network_manager_->get(request);
    connect(stream_info_reply_, &QNetworkReply::readyRead, this, &LiveStreamSourceBilibili::OnRequestStreamInfoProgress);
    connect(stream_info_reply_, &QNetworkReply::finished, this, &LiveStreamSourceBilibili::OnRequestStreamInfoComplete);
}

void LiveStreamSourceBilibili::onRequestDeactivate()
{
    EndData();
    emit RequestDeleteInputStream();
    if (av_reply_)
    {
        av_reply_->deleteLater();
        av_reply_ = nullptr;
    }
}

void LiveStreamSourceBilibili::OnRequestUpdateInfoProgress()
{
    if (!info_reply_)
        return;
    if (info_reply_->bytesAvailable() > 2048)
    {
        info_reply_->deleteLater();
        info_reply_ = nullptr;
        emit infoUpdated(-1, QList<QString>());
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
        emit infoUpdated(-1, QList<QString>());
        return;
    }
    QJsonObject json_object = json_doc.object();
    int code = (int)json_object.value("code").toDouble(-1);
    if (code != 0)
    {
        emit infoUpdated(-abs(code), QList<QString>());
        return;
    }
    QJsonValue data_value = json_object.value("data");
    if (!data_value.isObject())
    {
        emit infoUpdated(-1, QList<QString>());
        return;
    }
    QJsonObject data_object = data_value.toObject();

    QJsonValue room_id_value = data_object.value("room_id");
    if (!room_id_value.isDouble())
    {
        emit infoUpdated(-1, QList<QString>());
        return;
    }
    QJsonValue live_status_value = data_object.value("live_status");
    if (!room_id_value.isDouble())
    {
        emit infoUpdated(-1, QList<QString>());
        return;
    }
    room_id_ = (int)room_id_value.toDouble();
    int status = (int)live_status_value.toDouble();
    quality_.clear();

    QJsonValue qn_desc_value = data_object.value("playurl_info").toObject().value("playurl").toObject().value("g_qn_desc");
    if (!qn_desc_value.isArray())
    {
        emit infoUpdated(status, QList<QString>());
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
    emit infoUpdated(status, descs);
}

void LiveStreamSourceBilibili::OnRequestStreamInfoProgress()
{
    if (!stream_info_reply_)
        return;
    if (stream_info_reply_->bytesAvailable() > 2048)
    {
        stream_info_reply_->deleteLater();
        stream_info_reply_ = nullptr;
        emit InvalidSourceArgument();
        return;
    }
}

void LiveStreamSourceBilibili::OnRequestStreamInfoComplete()
{
    if (!stream_info_reply_)
        return;
    QJsonDocument json_doc = QJsonDocument::fromJson(stream_info_reply_->readAll());
    stream_info_reply_->deleteLater();
    stream_info_reply_ = nullptr;

    if (!json_doc.isObject())
    {
        emit InvalidSourceArgument();
        return;
    }
    QJsonObject json_object = json_doc.object();
    int code = (int)json_object.value("code").toDouble(-1);
    if (code != 0)
    {
        emit InvalidSourceArgument();
        return;
    }
    QJsonValue data_value = json_object.value("data");
    if (!data_value.isObject())
    {
        emit InvalidSourceArgument();
        return;
    }
    QJsonObject data_object = data_value.toObject();

    QJsonValue durl_value = data_object.value("durl");
    if (!durl_value.isArray())
    {
        emit InvalidSourceArgument();
        return;
    }
    QJsonArray durl = durl_value.toArray();
    if (durl.empty())
    {
        emit InvalidSourceArgument();
        return;
    }

    //TODO: switch between different url on failure
    QJsonValue first_url = durl[0].toObject().value("url");
    if (!first_url.isString())
    {
        emit InvalidSourceArgument();
        return;
    }

    QUrl request_url(first_url.toString());
    QNetworkRequest request(request_url);
    av_reply_ = network_manager_->get(request);

    //TODO: handle openChanged
    emit RequestNewInputStream("stream.flv");
    connect(av_reply_, &QNetworkReply::readyRead, this, &LiveStreamSourceBilibili::OnAVStreamProgress);
    connect(av_reply_, &QNetworkReply::finished, this, &LiveStreamSourceBilibili::OnAVStreamComplete);
    push_timer_->start(50);
}

void LiveStreamSourceBilibili::OnAVStreamProgress()
{
    if (!av_reply_)
    {
        push_timer_->stop();
        return;
    }
    if (!av_reply_->isOpen())
    {
        av_reply_->deleteLater();
        av_reply_ = nullptr;
        push_timer_->stop();
        return;
    }
    //TODO: detect failure
    PushData(av_reply_);
}

void LiveStreamSourceBilibili::OnAVStreamPush()
{
    if (!av_reply_)
    {
        push_timer_->stop();
        return;
    }
    if (!av_reply_->isOpen())
    {
        av_reply_->deleteLater();
        av_reply_ = nullptr;
        push_timer_->stop();
        return;
    }
    if (av_reply_->bytesAvailable() > 0)
        PushData(av_reply_);
}

void LiveStreamSourceBilibili::OnAVStreamComplete()
{
    if (!av_reply_)
    {
        push_timer_->stop();
        return;
    }
    qDebug() << av_reply_->errorString();
    av_reply_->deleteLater();
    av_reply_ = nullptr;
    push_timer_->stop();
    EndData();
}
