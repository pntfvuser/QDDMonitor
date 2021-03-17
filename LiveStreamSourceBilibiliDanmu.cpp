#include "pch.h"
#include "LiveStreamSourceBilibiliDanmu.h"

#include "LiveStreamSourceBilibili.h"

namespace
{

int ZlibDecompress(std::vector<char> &dst, const char *src, size_t &src_size)
{
    static constexpr size_t kUIntMax = std::numeric_limits<uInt>::max();
    static constexpr size_t kDstBlockSize = 0x1000;

    struct ManagedZStream
    {
        ~ManagedZStream() { if (initialized) inflateEnd(&stream); }

        z_stream stream;
        bool initialized = false;
    } managed_stream;
    z_stream &stream = managed_stream.stream;

    int err;
    size_t src_left = src_size;

    stream.next_in = (z_const Bytef *)src;
    stream.avail_in = 0;
    stream.zalloc = nullptr;
    stream.zfree = nullptr;
    stream.opaque = nullptr;

    err = inflateInit(&stream);
    if (err != Z_OK)
        return err;
    managed_stream.initialized = true;

    stream.next_out = nullptr;
    stream.avail_out = 0;

    do
    {
        if (stream.avail_out == 0)
        {
            size_t old_size = dst.size();
            try
            {
                dst.resize(old_size + kDstBlockSize);
            }
            catch (...) { return Z_MEM_ERROR; }
            stream.next_out = (Bytef *)(dst.data() + old_size);
            stream.avail_out = kDstBlockSize;
        }
        if (stream.avail_in == 0)
        {
            stream.avail_in = src_left > kUIntMax ? kUIntMax : (uInt)src_left;
            src_left -= stream.avail_in;
        }
        err = inflate(&stream, Z_NO_FLUSH);
    } while (err == Z_OK);

    src_size -= src_left + stream.avail_in;
    try
    {
        dst.resize(dst.size() - stream.avail_out);
    }
    catch (...) { return Z_MEM_ERROR; }

    return err == Z_STREAM_END ? Z_OK :
           err == Z_NEED_DICT ? Z_DATA_ERROR :
           err == Z_BUF_ERROR && src_left + stream.avail_out ? Z_DATA_ERROR :
           err;
}

}

LiveStreamSourceBilibiliDanmu::LiveStreamSourceBilibiliDanmu(QNetworkAccessManager *network_manager, LiveStreamSourceBilibili *parent)
    :QObject(parent), parent_(parent), network_manager_(network_manager), chatroom_heartbeat_timer_(new QTimer(this))
{
    connect(chatroom_heartbeat_timer_, &QTimer::timeout, this, &LiveStreamSourceBilibiliDanmu::OnChatroomSocketHeartbeatTick);
    chatroom_heartbeat_timer_->setInterval(30000);
}

void LiveStreamSourceBilibiliDanmu::Activate(int room_id, int retry_left)
{
    if (chatroom_info_reply_)
        return;
    if (chatroom_socket_)
    {
        reactivate_ = true;
        return;
    }

    if (retry_left < 0)
    {
        reactivate_ = false;
        return;
    }
    room_id_ = room_id;
    retry_left_ = retry_left - 1;

    QUrl request_url("https://api.live.bilibili.com/room/v1/Danmu/getConf");
    QUrlQuery request_query;
    request_query.addQueryItem("room_id", QString::number(room_id_));
    request_url.setQuery(request_query);

    QNetworkRequest request(request_url);
    chatroom_info_reply_ = network_manager_->get(request);
    if (!chatroom_info_reply_)
        return;
    connect(chatroom_info_reply_, &QNetworkReply::readyRead, this, &LiveStreamSourceBilibiliDanmu::OnRequestChatroomInfoProgress);
    connect(chatroom_info_reply_, &QNetworkReply::finished, this, &LiveStreamSourceBilibiliDanmu::OnRequestChatroomInfoComplete);
    reactivate_ = true;
}

void LiveStreamSourceBilibiliDanmu::Deactivate()
{
    if (!chatroom_socket_ && !chatroom_info_reply_)
        return;
    reactivate_ = false;
    if (chatroom_info_reply_)
    {
        chatroom_info_reply_->close();
        chatroom_info_reply_->deleteLater();
        chatroom_info_reply_ = nullptr;
    }
    if (chatroom_socket_)
    {
        chatroom_socket_->close();
    }
}

void LiveStreamSourceBilibiliDanmu::OnRequestChatroomInfoProgress()
{
    if (!chatroom_info_reply_ || sender() != chatroom_info_reply_)
        return;
    if (chatroom_info_reply_->bytesAvailable() > 2048)
    {
        chatroom_info_reply_->deleteLater();
        chatroom_info_reply_ = nullptr;

        if (reactivate_)
            Activate(room_id_, retry_left_);
        return;
    }
}

void LiveStreamSourceBilibiliDanmu::OnRequestChatroomInfoComplete()
{
    if (!chatroom_info_reply_ || sender() != chatroom_info_reply_)
        return;
    QJsonDocument json_doc = QJsonDocument::fromJson(chatroom_info_reply_->readAll());
    chatroom_info_reply_->deleteLater();
    chatroom_info_reply_ = nullptr;

    Q_ASSERT(!chatroom_socket_);
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

        QJsonValue token_value = data_object.value("token");
        if (!token_value.isString())
            break;
        token_ = token_value.toString();

        //TODO: cache hosts and switch between hosts on failure (OnChatroomSocketErrorOccurred is called and QWebSocket::state() is not QAbstractSocket::ConnectedState)

        QJsonValue host_server_list_value = data_object.value("host_server_list");
        if (!host_server_list_value.isArray())
            break;
        QJsonArray host_server_list = host_server_list_value.toArray();
        if (host_server_list.empty())
            break;

        QJsonValue selected_host_server_value = host_server_list[0];
        if (!selected_host_server_value.isObject())
            break;
        QJsonObject selected_host_server = selected_host_server_value.toObject();

        QJsonValue server_host = selected_host_server.value("host"), server_port = selected_host_server.value("wss_port");
        if (!server_host.isString() || !server_port.isDouble())
            break;
        QUrl server_url;
        server_url.setScheme("wss");
        server_url.setHost(server_host.toString());
        server_url.setPort((int)server_port.toDouble());
        server_url.setPath("/sub");

        qDebug() << "Connecting to chatroom";
        chatroom_socket_ = new QWebSocket(QString(), QWebSocketProtocol::VersionLatest, this);
        if (!chatroom_socket_)
            break;
        chatroom_socket_->open(server_url);
        connect(chatroom_socket_, QOverload<QAbstractSocket::SocketError>::of(&QWebSocket::error), this, &LiveStreamSourceBilibiliDanmu::OnChatroomSocketErrorOccurred);
        connect(chatroom_socket_, &QWebSocket::connected, this, &LiveStreamSourceBilibiliDanmu::OnChatroomSocketConnected);
        connect(chatroom_socket_, &QWebSocket::disconnected, this, &LiveStreamSourceBilibiliDanmu::OnChatroomSocketDisconnected);
    } while (false);
    if (!chatroom_socket_)
    {
        if (reactivate_)
            Activate(room_id_, retry_left_);
        return;
    }
}

void LiveStreamSourceBilibiliDanmu::OnChatroomSocketErrorOccurred(QAbstractSocket::SocketError socket_error)
{
    if (!chatroom_socket_)
        return;
    if (chatroom_socket_->state() == QAbstractSocket::ConnectedState)
        return; //Will be handled in OnChatroomSocketDisconnected
    qWarning() << "Chatroom socket connection failed, code " << socket_error;
    chatroom_socket_->deleteLater();
    chatroom_socket_ = nullptr;
    if (reactivate_)
        Activate(room_id_, retry_left_);
}

void LiveStreamSourceBilibiliDanmu::OnChatroomSocketConnected()
{
    if (!chatroom_socket_)
        return;
    qDebug() << "Chatroom socket connected";
    connect(chatroom_socket_, &QWebSocket::binaryMessageReceived, this, &LiveStreamSourceBilibiliDanmu::OnChatroomSocketBinaryMessage);

    QJsonObject verify_data_object;
    verify_data_object["uid"] = 0;
    verify_data_object["roomid"] = room_id_;
    verify_data_object["protover"] = 2;
    verify_data_object["platform"] = "web";
    verify_data_object["clientver"] = "1.17.0";
    verify_data_object["type"] = 2;
    verify_data_object["key"] = token_;
    QByteArray verify_data = QJsonDocument(verify_data_object).toJson(QJsonDocument::Compact);

    QByteArray message;
    size_t header_offset = MakePacket(message, PROTOCOL_VERSION_HEARTBEAT, PACKET_VERIFY, verify_data.size());
    memcpy(message.data() + header_offset, verify_data.data(), verify_data.size());
    chatroom_socket_->sendBinaryMessage(message);
    chatroom_heartbeat_timer_->start();
    OnChatroomSocketHeartbeatTick();
}

void LiveStreamSourceBilibiliDanmu::OnChatroomSocketDisconnected()
{
    if (!chatroom_socket_)
        return;
    qDebug() << "Chatroom socket disconnected";
    chatroom_socket_->deleteLater();
    chatroom_socket_ = nullptr;
    if (reactivate_)
        Activate(room_id_, retry_left_);
}

void LiveStreamSourceBilibiliDanmu::OnChatroomSocketHeartbeatTick()
{
    if (!chatroom_socket_ || chatroom_socket_->state() != QAbstractSocket::ConnectedState)
    {
        chatroom_heartbeat_timer_->stop();
        return;
    }
    static constexpr const char kHeartbeatMessage[] = "\x00\x00\x00\x1F\x00\x10\x00\x01\x00\x00\x00\x02\x00\x00\x00\x01\x5B\x6F\x62\x6A\x65\x63\x74\x20\x4F\x62\x6A\x65\x63\x74\x5D";
    chatroom_socket_->sendBinaryMessage(QByteArray::fromRawData(kHeartbeatMessage, sizeof(kHeartbeatMessage) - 1));
}

void LiveStreamSourceBilibiliDanmu::OnChatroomSocketBinaryMessage(const QByteArray &message)
{
    if (!chatroom_socket_)
        return;

    if (message.size() < (int)kPacketHeaderSize)
        return;
    PacketHeader message_header;
    DecodePacketHeader(message_header, message.data());

    const char *payload = nullptr;
    size_t payload_size = 0;
    std::vector<char> uncompressed_data;
    if (message_header.protocol_version == PROTOCOL_VERSION_ZLIB_JSON)
    {
        size_t src_size = message.size() - kPacketHeaderSize;
        int err = ZlibDecompress(uncompressed_data, message.data() + kPacketHeaderSize, src_size);
        if (err != Z_OK)
        {
            qWarning() << "Error while decompression, code " << err;
            return;
        }
        payload = uncompressed_data.data();
        payload_size = uncompressed_data.size();
    }
    else
    {
        payload = message.data() + kPacketHeaderSize;
        payload_size = message.size() - kPacketHeaderSize;
    }

    while (payload_size >= kPacketHeaderSize)
    {
        PacketHeader packet_header;
        DecodePacketHeader(packet_header, payload);
        if (packet_header.size_total < kPacketHeaderSize)
            break;
        size_t packet_size = std::min<size_t>(payload_size, packet_header.size_total);

        const char *packet_payload = payload + kPacketHeaderSize;
        size_t packet_payload_size = packet_size - kPacketHeaderSize;

        switch (packet_header.protocol_version)
        {
        case PROTOCOL_VERSION_RAW_JSON:
        case PROTOCOL_VERSION_ZLIB_JSON:
        {
            if (packet_payload_size > (size_t)std::numeric_limits<int>::max())
                break;
            QJsonDocument json_doc = QJsonDocument::fromJson(QByteArray::fromRawData(packet_payload, packet_payload_size));
            if (!json_doc.isObject())
                break;
            QJsonObject packet_object = json_doc.object();
            QString command = packet_object.value("cmd").toString();

            //TODO: Use a QHash<QString, ?> instead of a bunch of if. Even QHash<QString, int> and a switch would be better
            if (command == "DANMU_MSG")
            {
                QJsonArray info = packet_object.value("info").toArray();
                if (info.size() < 2)
                    break;

                QString content = info[1].toString();
                if (content.isEmpty())
                    break;
                QSharedPointer<SubtitleFrame> danmu_frame = QSharedPointer<SubtitleFrame>::create();
                danmu_frame->content = content;

                QJsonArray style = info[0].toArray();
                //qDebug() << style;
                if (style.size() >= 4)
                {
                    quint32 color_value = (quint32)style[3].toDouble(0xFFFFFF);
                    danmu_frame->color = QColor((quint8)(color_value >> 16), (quint8)(color_value >> 8), (quint8)(color_value), 0xFF);
                    danmu_frame->style = SubtitleStyle::NORMAL;
                }
                else
                {
                    danmu_frame->color = QColor(0xFF, 0xFF, 0xFF, 0xFF);
                    danmu_frame->style = SubtitleStyle::NORMAL;
                }
                parent_->OnNewDanmu(danmu_frame);
            }

            break;
        }
        case PROTOCOL_VERSION_HEARTBEAT:
        {
            //Not going to deal with PACKET_HEARTBEAT_RESPONSE since we don't need that for now
            if (packet_header.packet_type == PACKET_VERIFY_RESPONSE_SUCCESS)
            {
                QJsonDocument json_doc = QJsonDocument::fromJson(QByteArray::fromRawData(packet_payload, packet_payload_size));
                if (!json_doc.isObject())
                    break;
                QJsonObject packet_object = json_doc.object();
                if (packet_object.value("code").toDouble(-1) != 0)
                {
                    qWarning() << "Chatfoom verification failed";
                    chatroom_socket_->close();
                    return;
                }
                else
                {
                    qDebug() << "Chatfoom verification succeeded";
                }
            }
            break;
        }
        }

        payload += packet_size;
        payload_size -= packet_size;
    }
}

size_t LiveStreamSourceBilibiliDanmu::MakePacket(QByteArray &buffer, ProtocolVersion protocol_version, PacketType packet_type, size_t payload_size)
{
    size_t size_total = kPacketHeaderSize + payload_size;
    buffer.resize(size_total);
    qToBigEndian<quint32>(size_total, buffer.data());
    qToBigEndian<quint16>(16, buffer.data() + 4);
    qToBigEndian<quint16>(protocol_version, buffer.data() + 6);
    qToBigEndian<quint32>(packet_type, buffer.data() + 8);
    qToBigEndian<quint32>(1, buffer.data() + 12);
    return kPacketHeaderSize;
}

void LiveStreamSourceBilibiliDanmu::PreparePacket(QByteArray &buffer, ProtocolVersion protocol_version, PacketType packet_type)
{
    buffer.resize(kPacketHeaderSize);
    qToBigEndian<quint16>(16, buffer.data() + 4);
    qToBigEndian<quint16>(protocol_version, buffer.data() + 6);
    qToBigEndian<quint32>(packet_type, buffer.data() + 8);
    qToBigEndian<quint32>(1, buffer.data() + 12);
}

void LiveStreamSourceBilibiliDanmu::FinishPacket(QByteArray &buffer)
{
    qToBigEndian<quint32>(buffer.size(), buffer.data());
}

void LiveStreamSourceBilibiliDanmu::DecodePacketHeader(PacketHeader &header, const char *src)
{
    header.size_total = qFromBigEndian<quint32>(src);
    header.reserved_16 = qFromBigEndian<quint16>(src + 4);
    header.protocol_version = qFromBigEndian<quint16>(src + 6);
    header.packet_type = qFromBigEndian<quint32>(src + 8);
    header.reserved_1 = qFromBigEndian<quint32>(src + 12);
}
