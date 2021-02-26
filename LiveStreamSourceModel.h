#ifndef LIVESTREAMSOURCEMODEL_H
#define LIVESTREAMSOURCEMODEL_H

class LiveStreamSource;

class LiveStreamSourceInfo
{
    Q_GADGET

    Q_PROPERTY(int id READ id)
    Q_PROPERTY(QString name READ name)
    Q_PROPERTY(LiveStreamSource source READ source)
public:
    LiveStreamSourceInfo() = default;
    LiveStreamSourceInfo(int id, const QString &name, LiveStreamSource *source) :id_(id), name_(name), source_(source) {}

    int id() const { return id_; }
    const QString &name() const { return name_; }
    LiveStreamSource *source() const { return source_; }
private:
    int id_;
    QString name_;
    LiveStreamSource *source_;
};

class LiveStreamSourceModel : public QAbstractListModel
{
    Q_OBJECT
public:
    explicit LiveStreamSourceModel(QObject *parent = nullptr);
    ~LiveStreamSourceModel();

    int rowCount(const QModelIndex & = QModelIndex()) const override;
    QVariant data(const QModelIndex &index, int role) const override;

    Q_INVOKABLE void addBilibiliSource(const QString &name, int room_display_id);
    Q_INVOKABLE void removeSourceById(int id);
    Q_INVOKABLE void removeSourceByIndex(int index);
private:
    void addSource(const QString &name, LiveStreamSource *source);

    QThread source_thread_;
    std::vector<int> sources_index_;
    std::unordered_map<int, LiveStreamSourceInfo> sources_;
    int next_id_ = 0;
};

#endif // LIVESTREAMSOURCEMODEL_H
