#ifndef LIVESTREAMSOURCEBILIBILIDANMU_H
#define LIVESTREAMSOURCEBILIBILIDANMU_H

class LiveStreamSourceBilibili;

class LiveStreamSourceBilibiliDanmu : public QObject
{
    Q_OBJECT

    enum ProtocolVersion
    {
        PROTOCOL_VERSION_RAW_JSON = 0,
        PROTOCOL_VERSION_HEARTBEAT = 1,
        PROTOCOL_VERSION_ZLIB_JSON = 2,
    };

    enum PacketType
    {
        PACKET_HEARTBEAT = 2,
        PACKET_HEARTBEAT_RESPONSE = 3,
        PACKET_NOTICE = 5,
        PACKET_VERIFY = 7,
        PACKET_VERIFY_RESPONSE_SUCCESS = 8,
    };

    static constexpr size_t kPacketHeaderSize = 4 + 2 + 2 + 4 + 4;
    struct PacketHeader
    {
        quint32 size_total;
        quint16 reserved_16, protocol_version;
        quint32 packet_type, reserved_1;
    };
public:
    explicit LiveStreamSourceBilibiliDanmu(QNetworkAccessManager *network_manager, LiveStreamSourceBilibili *parent);

    void Activate(int room_id, int retry_left);
    void Deactivate();
private slots:
    void OnRequestChatroomInfoProgress();
    void OnRequestChatroomInfoComplete();

    void OnChatroomSocketErrorOccurred(QAbstractSocket::SocketError socket_error);
    void OnChatroomSocketConnected();
    void OnChatroomSocketDisconnected();
    void OnChatroomSocketHeartbeatTick();
    void OnChatroomSocketBinaryMessage(const QByteArray &message);
private:
    size_t MakePacket(QByteArray &buffer, ProtocolVersion protocol_version, PacketType packet_type, size_t payload_size);
    void PreparePacket(QByteArray &buffer, ProtocolVersion protocol_version, PacketType packet_type);
    void FinishPacket(QByteArray &buffer);
    void DecodePacketHeader(PacketHeader &header, const char *src);

    LiveStreamSourceBilibili *parent_ = nullptr;

    int room_id_ = -1, retry_left_ = -1;
    QString token_;
    bool reactivate_ = false;

    QNetworkAccessManager *network_manager_ = nullptr;
    QNetworkReply *chatroom_info_reply_ = nullptr;
    QWebSocket *chatroom_socket_ = nullptr;
    QTimer *chatroom_heartbeat_timer_ = nullptr;
};

#endif // LIVESTREAMSOURCEBILIBILIDANMU_H
