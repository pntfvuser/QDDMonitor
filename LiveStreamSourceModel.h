#ifndef LIVESTREAMSOURCEMODEL_H
#define LIVESTREAMSOURCEMODEL_H

class LiveStreamSource;

class LiveStreamSourceInfo
{
    Q_GADGET

    Q_PROPERTY(int id READ id)
    Q_PROPERTY(LiveStreamSource *source READ source)
    Q_PROPERTY(QString name READ name)
    Q_PROPERTY(QString description READ description)
    Q_PROPERTY(QString option READ option)
    Q_PROPERTY(QList<QString> availableOptions READ availableOptions)
    Q_PROPERTY(bool online READ online)
    Q_PROPERTY(bool activated READ activated)
public:
    LiveStreamSourceInfo() = default;
    LiveStreamSourceInfo(int id, LiveStreamSource *source, const QString &name) :id_(id), source_(source), name_(name) {}

    int id() const { return id_; }
    LiveStreamSource *source() const { return source_; }

    const QString &name() const { return name_; }
    void setName(const QString &new_name) { name_ = new_name; }
    const QString &description() const { return description_; }
    void setDescription(const QString &new_description) { description_ = new_description; }
    const QString &option() const { return option_; }
    void setOption(const QString &new_option) { option_ = new_option; }
    const QList<QString> &availableOptions() const { return available_options_; }
    void setAvailableOptions(const QList<QString> &new_available_options) { available_options_ = new_available_options; }
    bool online() const { return online_; }
    void setOnline(bool new_online) { online_ = new_online; }
    bool activated() const { return activated_; }
    void setActivated(bool new_activated) { activated_ = new_activated; }

    QString effectiveOption() const { return option_.isEmpty() ? (available_options_.empty() ? "" : available_options_.front()) : option_; }
private:
    int id_;
    LiveStreamSource *source_;
    QString name_, description_, option_;
    QList<QString> available_options_;
    bool online_ = false, activated_ = false;
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
    Q_INVOKABLE void setSourceOption(int id, const QString &option);
    Q_INVOKABLE void removeSourceById(int id);
    Q_INVOKABLE void removeSourceByIndex(int index);

    void activateAndGetSources(std::vector<std::pair<int, LiveStreamSource *>> &sources);
signals:
    void newSource(int id);
    void deleteSource(int id);
public slots:
    void UpdateSingleSourceDone(int status, const QString &description, const QList<QString> &options);
private:
    void AddSource(LiveStreamSource *source, const QString &name);
    int FindSourceIndex(int id);

    void OnActivated(int id);
    void OnDeactivated(int id);

    void StartUpdateSources();
    void ContinueUpdateSources();
    void UpdateSingleSourceCanceled();

    void ActivateSource(LiveStreamSource *source, const QString &option);
    void DeactivateSource(LiveStreamSource *source);

    QThread source_thread_;
    QNetworkAccessManager *source_network_manager_ = nullptr;

    std::unordered_map<int, LiveStreamSourceInfo> sources_;
    std::vector<int> sources_index_;
    int sources_offline_pos_ = 0;
    int next_id_ = 0;

    std::unordered_set<int> activated_sources_;

    std::vector<int> sources_updating_;
    int sources_updated_count_ = -1;
};

#endif // LIVESTREAMSOURCEMODEL_H
