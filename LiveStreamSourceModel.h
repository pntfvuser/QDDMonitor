#ifndef LIVESTREAMSOURCEMODEL_H
#define LIVESTREAMSOURCEMODEL_H

class LiveStreamSource;

class LiveStreamSourceInfo : public QObject
{
    Q_OBJECT

    Q_PROPERTY(int id READ id CONSTANT)
    Q_PROPERTY(LiveStreamSource *source READ source CONSTANT)
    Q_PROPERTY(QString name READ name NOTIFY nameChanged)
    Q_PROPERTY(QString description READ description NOTIFY descriptionChanged)
    Q_PROPERTY(QUrl cover READ cover NOTIFY coverChanged)
    Q_PROPERTY(int optionIndex READ optionIndex NOTIFY optionIndexChanged)
    Q_PROPERTY(QList<QString> availableOptions READ availableOptions NOTIFY availableOptionsChanged)
    Q_PROPERTY(bool online READ online NOTIFY onlineChanged)
    Q_PROPERTY(bool activated READ activated NOTIFY activatedChanged)
    Q_PROPERTY(bool recording READ recording NOTIFY recordingChanged)

    Q_PROPERTY(QString effectiveOption READ effectiveOption NOTIFY effectiveOptionChanged)
public:
    explicit LiveStreamSourceInfo(QObject *parent = nullptr) :QObject(parent) {}
    LiveStreamSourceInfo(int id, LiveStreamSource *source, const QString &name, QObject *parent = nullptr) :QObject(parent), id_(id), source_(source), name_(name) {}

    int id() const { return id_; }
    LiveStreamSource *source() const { return source_; }

    const QString &name() const { return name_; }
    void setName(const QString &new_name) { if (name_ != new_name) { name_ = new_name; emit nameChanged(); } }
    const QString &description() const { return description_; }
    void setDescription(const QString &new_description) { if (description_ != new_description) { description_ = new_description; emit descriptionChanged(); } }
    const QUrl &cover() const { return cover_; }
    void setCover(const QUrl &new_cover) { if (cover_ != new_cover) { cover_ = new_cover; emit coverChanged(); } }
    int optionIndex() const { return option_index_; }
    void setOptionIndex(int new_option_index);
    const QList<QString> &availableOptions() const { return available_options_; }
    void setAvailableOptions(const QList<QString> &new_available_options);
    bool online() const { return online_; }
    void setOnline(bool new_online) { if (online_ != new_online) { online_ = new_online; emit onlineChanged(); } }
    bool activated() const { return activated_; }
    void setActivated(bool new_activated) { if (activated_ != new_activated) { activated_ = new_activated; emit activatedChanged(); } }
    bool recording() { return recording_; }
    void setRecording(bool new_recording) { if (recording_ != new_recording) { recording_ = new_recording; emit recordingChanged(); } }

    QString effectiveOption() const;
signals:
    void nameChanged();
    void descriptionChanged();
    void coverChanged();
    void optionIndexChanged();
    void availableOptionsChanged();
    void onlineChanged();
    void activatedChanged();
    void recordingChanged();

    void effectiveOptionChanged();
private:
    int id_;
    LiveStreamSource *source_;
    QString name_, description_;
    QUrl cover_;
    int option_index_ = -1;
    QList<QString> available_options_;
    bool online_ = false, activated_ = false, recording_ = false;
};

class LiveStreamSourceModel : public QAbstractListModel
{
    Q_OBJECT
public:
    explicit LiveStreamSourceModel(QObject *parent = nullptr);
    ~LiveStreamSourceModel();

    int rowCount(const QModelIndex & = QModelIndex()) const override;
    QVariant data(const QModelIndex &index, int role) const override;

    Q_INVOKABLE void addFileSource(const QString &name, const QString &path);
    Q_INVOKABLE void addBilibiliSource(const QString &name, int room_display_id);
    Q_INVOKABLE void setSourceOption(int id, int option_index);
    Q_INVOKABLE void setSourceRecording(int id, bool enabled);
    Q_INVOKABLE void removeSourceById(int id);
    Q_INVOKABLE void removeSourceByIndex(int index);

    LiveStreamSourceInfo *ActivateAndGetSource(int source_id);
    void DeactivateSingleSource(int source_id);
    void ActivateAndGetSources(std::vector<std::pair<int, LiveStreamSourceInfo *>> &sources);
    void DeactivateAllSource();
signals:
    void newSource(int id);
    void deleteSource(int id);
public slots:
    void UpdateSingleSourceDone(int status, const QString &description, const QUrl &cover, const QList<QString> &options);
private:
    void AddSource(LiveStreamSource *source, const QString &name);
    int FindSourceIndex(int id);

    void OnActivated(int id);
    void OnDeactivated(int id);

    void StartUpdateSources();
    void ContinueUpdateSources();
    void UpdateSingleSourceCanceled();

    static void ActivateSource(LiveStreamSource *source, const QString &option);
    static void DeactivateSource(LiveStreamSource *source);
    static void EnableSourceRecording(LiveStreamSource *source, const QString &out_path);
    static void DisableSourceRecording(LiveStreamSource *source);

    void LoadFromFile();
    void SaveToFile();

    QThread source_thread_;
    QNetworkAccessManager *source_network_manager_ = nullptr;

    std::unordered_map<int, std::unique_ptr<LiveStreamSourceInfo>> sources_;
    std::vector<int> sources_index_;
    int sources_offline_pos_ = 0;
    int next_id_ = 0;

    std::unordered_set<int> activated_sources_;

    std::vector<int> sources_updating_;
    int sources_updated_count_ = -1;
};

#endif // LIVESTREAMSOURCEMODEL_H
