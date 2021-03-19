#include "pch.h"
#include "LiveStreamSourceModel.h"

#include "LiveStreamSource.h"
#include "LiveStreamSourceBilibili.h"
#include "LiveStreamSourceFile.h"

Q_LOGGING_CATEGORY(CategorySourceControl, "qddm.sourcectrl")

void LiveStreamSourceInfo::setOptionIndex(int new_option_index)
{
    if (new_option_index < 0 || new_option_index >= available_options_.size())
        new_option_index = available_options_.empty() ? -1 : 0;
    if (option_index_ != new_option_index)
    {
        QString old_effective_option = effectiveOption();

        option_index_ = new_option_index;
        emit optionIndexChanged();

        if (old_effective_option != effectiveOption())
            emit effectiveOptionChanged();
    }
}

void LiveStreamSourceInfo::setAvailableOptions(const QList<QString> &new_available_options)
{
    if (available_options_ != new_available_options)
    {
        QString old_effective_option = effectiveOption();

        if (option_index_ >= new_available_options.size())
        {
            Q_ASSERT(!available_options_.empty());
            option_index_ = new_available_options.empty() ? -1 : 0;
            emit optionIndexChanged();
        }
        available_options_ = new_available_options;
        emit availableOptionsChanged();
        if (option_index_ == -1 && !available_options_.empty())
        {
            option_index_ = 0;
            emit optionIndexChanged();
        }

        if (old_effective_option != effectiveOption())
            emit effectiveOptionChanged();
    }
}

QString LiveStreamSourceInfo::effectiveOption() const
{
    if (option_index_ != -1)
        return available_options_[option_index_];
    if (!available_options_.empty())
        return available_options_.front();
    return QString();
}

LiveStreamSourceModel::LiveStreamSourceModel(QObject *parent)
    :QAbstractListModel(parent), source_network_manager_(new QNetworkAccessManager)
{
    source_network_manager_->moveToThread(&source_thread_);
    connect(&source_thread_, &QThread::finished, source_network_manager_, &QObject::deleteLater);
    source_thread_.start();

    LoadFromFile();

    connect(this, &LiveStreamSourceModel::deleteSource, this, [this](int id)
    {
        if (sources_updated_count_ != -1 && sources_updating_[sources_updated_count_] == id)
        {
            UpdateSingleSourceCanceled();
        }
    });
    StartUpdateSources();
}

LiveStreamSourceModel::~LiveStreamSourceModel()
{
    SaveToFile(); //Sources will be deleted when source_thread_ is finished

    source_thread_.exit();
    source_thread_.wait();
}

int LiveStreamSourceModel::rowCount(const QModelIndex &) const
{
    return sources_.size();
}

QVariant LiveStreamSourceModel::data(const QModelIndex &index, int role) const
{
    switch (role)
    {
    case Qt::DisplayRole:
        if (index.column() == 0 && index.row() >= 0 && index.row() < (int)sources_index_.size())
        {
            int id = sources_index_[index.row()];
            auto itr = sources_.find(id);
            if (itr != sources_.end())
                return QVariant::fromValue(itr->second.get());
        }
        break;
    default:
        break;
    }

    return QVariant();
}

void LiveStreamSourceModel::addFileSource(const QString &name, const QString &path)
{
    AddSource(new LiveStreamSourceFile(path), name);
}

void LiveStreamSourceModel::addBilibiliSource(const QString &name, int room_display_id)
{
    AddSource(new LiveStreamSourceBilibili(room_display_id, source_network_manager_), name);
}

void LiveStreamSourceModel::setSourceOption(int id, int option_index)
{
    auto itr = sources_.find(id);
    if (itr != sources_.end())
    {
        LiveStreamSourceInfo *source_info = itr->second.get();
        QString effective_option = source_info->effectiveOption();
        source_info->setOptionIndex(option_index);
        if (effective_option != source_info->effectiveOption())
            DeactivateSource(source_info->source());
    }
}

void LiveStreamSourceModel::setSourceRecording(int id, bool enabled)
{
    auto itr = sources_.find(id);
    if (itr != sources_.end())
    {
        LiveStreamSourceInfo *source_info = itr->second.get();
        if (source_info->recording() != enabled)
        {
            //TODO: support set record path
            if (enabled)
                EnableSourceRecording(source_info->source(), ".");
            else
                DisableSourceRecording(source_info->source());
            source_info->setRecording(enabled);
        }
    }
}

void LiveStreamSourceModel::clearSourceBuffer(int id)
{
    auto itr = sources_.find(id);
    if (itr != sources_.end())
    {
        LiveStreamSourceInfo *source_info = itr->second.get();
        ClearSourceBuffer(source_info->source());
    }
}

void LiveStreamSourceModel::removeSourceById(int id)
{
    auto itr = sources_.find(id);
    if (itr != sources_.end())
    {
        for (int index = 0; index < (int)sources_index_.size(); ++index)
        {
            if (sources_index_[index] == id)
            {
                beginRemoveRows(QModelIndex(), index, index);
                sources_index_.erase(sources_index_.begin() + index);
                if (index < sources_offline_pos_)
                    --sources_offline_pos_;
                endRemoveRows();
            }
        }

        emit deleteSource(id);
        activated_sources_.erase(id);
        itr->second->source()->deleteLater();
        sources_.erase(itr);
    }
}

void LiveStreamSourceModel::removeSourceByIndex(int index)
{
    if (index >= 0 && index < (int)sources_index_.size())
    {
        int id = sources_index_[index];

        beginRemoveRows(QModelIndex(), index, index);
        sources_index_.erase(sources_index_.begin() + index);
        if (index < sources_offline_pos_)
            --sources_offline_pos_;
        endRemoveRows();

        auto itr = sources_.find(id);
        if (itr != sources_.end())
        {
            emit deleteSource(id);
            activated_sources_.erase(id);
            itr->second->source()->deleteLater();
            sources_.erase(itr);
        }
    }
}

LiveStreamSourceInfo *LiveStreamSourceModel::ActivateAndGetSource(int source_id)
{
    if (activated_sources_.count(source_id) > 0)
        return nullptr;
    auto itr = sources_.find(source_id);
    if (itr == sources_.end())
        return nullptr;
    activated_sources_.emplace(source_id);

    LiveStreamSourceInfo *source_info = itr->second.get();
    if (source_info->online() && !source_info->activated())
        ActivateSource(source_info->source(), source_info->effectiveOption());

    return source_info;
}

void LiveStreamSourceModel::DeactivateSingleSource(int source_id)
{
    if (activated_sources_.erase(source_id) > 0)
    {
        auto itr = sources_.find(source_id);
        if (itr != sources_.end() && itr->second->activated())
            DeactivateSource(itr->second->source());
    }
}

void LiveStreamSourceModel::ActivateAndGetSources(std::vector<std::pair<int, LiveStreamSourceInfo *>> &sources)
{
    std::unordered_set<int> new_activated_sources;
    for (auto &p : sources)
    {
        auto itr = sources_.find(p.first);
        if (itr == sources_.end())
        {
            p.second = nullptr;
        }
        else
        {
            new_activated_sources.insert(p.first);
            LiveStreamSourceInfo *source_info = itr->second.get();
            p.second = source_info;

            auto itr_activated = activated_sources_.find(p.first);
            if (itr_activated == activated_sources_.end())
            {
                //Not previously marked as activated
                if (source_info->online() && !source_info->activated())
                {
                    //Not activated yet and online, activate
                    ActivateSource(source_info->source(), source_info->effectiveOption());
                }
            }
            else
            {
                //Unmark it now so that everything left in activated_sources_ should be deactivated
                activated_sources_.erase(itr_activated);
            }
        }
    }
    for (int id : activated_sources_)
    {
        auto itr = sources_.find(id);
        if (itr != sources_.end() && itr->second->activated())
        {
            DeactivateSource(itr->second->source());
        }
    }
    activated_sources_ = std::move(new_activated_sources);
}

void LiveStreamSourceModel::DeactivateAllSource()
{
    for (int id : activated_sources_)
    {
        auto itr = sources_.find(id);
        if (itr != sources_.end() && itr->second->activated())
            DeactivateSource(itr->second->source());
    }
    activated_sources_.clear();
}

void LiveStreamSourceModel::AddSource(LiveStreamSource *source, const QString &name)
{
    int id = next_id_++;
    source->moveToThread(&source_thread_);
    connect(source, &LiveStreamSource::activated, this, [this, id]() { OnActivated(id); });
    connect(source, &LiveStreamSource::deactivated, this, [this, id]() { OnDeactivated(id); });
    connect(&source_thread_, &QThread::finished, source, &QObject::deleteLater);
    sources_.emplace(id, std::make_unique<LiveStreamSourceInfo>(id, source, name, this));

    int index = (int)sources_index_.size();
    beginInsertRows(QModelIndex(), index, index);
    sources_index_.push_back(id);
    endInsertRows();
    emit newSource(id);
}

int LiveStreamSourceModel::FindSourceIndex(int id)
{
    for (int i = 0; i < (int)sources_index_.size(); ++i)
        if (sources_index_[i] == id)
            return i;
    return -1;
}

void LiveStreamSourceModel::OnActivated(int id)
{
    auto itr = sources_.find(id);
    if (itr == sources_.end())
        return;
    itr->second->setActivated(true);
    qCDebug(CategorySourceControl, "Source %d activated", id);

    if (activated_sources_.count(id) == 0)
        DeactivateSource(itr->second->source());
}

void LiveStreamSourceModel::OnDeactivated(int id)
{
    auto itr = sources_.find(id);
    if (itr == sources_.end())
        return;
    itr->second->setActivated(false);
    qCDebug(CategorySourceControl, "Source %d deactivated", id);

    if (itr->second->online() && activated_sources_.count(id) > 0)
        ActivateSource(itr->second->source(), itr->second->effectiveOption());
}

void LiveStreamSourceModel::StartUpdateSources()
{
    if (sources_updated_count_ != -1)
        return;
    sources_updating_.clear();
    for (const auto &p : sources_)
        sources_updating_.push_back(p.first);
    sources_updated_count_ = 0;

    ContinueUpdateSources();
}

void LiveStreamSourceModel::ContinueUpdateSources()
{
    if (sources_updated_count_ >= (int)sources_updating_.size())
    {
        sources_updated_count_ = -1;
        QTimer::singleShot(1000, this, &LiveStreamSourceModel::StartUpdateSources);
        return;
    }
    int id = sources_updating_[sources_updated_count_];
    auto itr = sources_.find(id);
    if (itr == sources_.end())
    {
        UpdateSingleSourceCanceled();
        return;
    }
    LiveStreamSource *source = itr->second->source();
    connect(source, &LiveStreamSource::infoUpdated, this, &LiveStreamSourceModel::UpdateSingleSourceDone);
    QMetaObject::invokeMethod(source, "onRequestUpdateInfo");
}

void LiveStreamSourceModel::UpdateSingleSourceDone(int status, const QString &description, const QUrl &cover, const QList<QString> &options)
{
    if (sources_updated_count_ == -1)
        return;
    Q_ASSERT(sources_updated_count_ < (int)sources_updating_.size());
    int id = sources_updating_[sources_updated_count_];
    auto itr = sources_.find(id);
    if (itr == sources_.end())
    {
        UpdateSingleSourceCanceled();
        return;
    }
    LiveStreamSourceInfo &source_info = *itr->second;
    LiveStreamSource *source = source_info.source();
    if (sender() != source_info.source())
    {
        return;
    }
    disconnect(source, &LiveStreamSource::infoUpdated, this, &LiveStreamSourceModel::UpdateSingleSourceDone);

    source_info.setDescription(description);
    source_info.setCover(cover);
    source_info.setAvailableOptions(options);

    if (status == LiveStreamSource::STATUS_ONLINE && source_info.online() == false)
    {
        source_info.setOnline(true);
        //If marked as activated but not active, activate
        if (!source_info.activated() && activated_sources_.count(id) > 0)
        {
            ActivateSource(source_info.source(), source_info.effectiveOption());
        }

        int i = FindSourceIndex(id);
        if (i != -1)
        {
            //Move its position if needed
            if (i >= sources_offline_pos_)
            {
                Q_ASSERT(sources_index_[i] == id);
                int j;
                for (j = 0; j < sources_offline_pos_; ++j)
                    if (id < sources_index_[j])
                        break;
                if (i != j)
                {
                    bool ok = beginMoveRows(QModelIndex(), i, i, QModelIndex(), j);
                    Q_ASSERT(ok);
                    if (ok)
                    {
                        for (int p = i; p > j; --p)
                            sources_index_[p] = sources_index_[p - 1];
                        sources_index_[j] = id;
                        endMoveRows();
                    }
                }
                sources_offline_pos_ += 1;
            }
        }
    }
    else if (status != LiveStreamSource::STATUS_ONLINE && source_info.online() == true)
    {
        source_info.setOnline(false);

        int i = FindSourceIndex(id);
        if (i != -1)
        {
            //Move its position if needed
            if (i < sources_offline_pos_)
            {
                Q_ASSERT(sources_index_[i] == id);
                int j;
                for (j = sources_offline_pos_; j < (int)sources_index_.size(); ++j)
                    if (id < sources_index_[j])
                        break;
                if (i + 1 != j)
                {
                    bool ok = beginMoveRows(QModelIndex(), i, i, QModelIndex(), j);
                    Q_ASSERT(ok);
                    if (ok)
                    {
                        j -= 1;
                        for (int p = i; p < j; ++p)
                            sources_index_[p] = sources_index_[p + 1];
                        sources_index_[j] = id;
                        endMoveRows();
                    }
                }
                sources_offline_pos_ -= 1;
            }
        }
    }

    sources_updated_count_ += 1;
    ContinueUpdateSources();
}

void LiveStreamSourceModel::UpdateSingleSourceCanceled()
{
    sources_updated_count_ += 1;
    ContinueUpdateSources();
}

void LiveStreamSourceModel::ActivateSource(LiveStreamSource *source, const QString &option)
{
    QMetaObject::invokeMethod(source, "onRequestActivate", Q_ARG(QString, option));
}

void LiveStreamSourceModel::DeactivateSource(LiveStreamSource *source)
{
    QMetaObject::invokeMethod(source, "onRequestDeactivate");
}

void LiveStreamSourceModel::ClearSourceBuffer(LiveStreamSource *source)
{
    QMetaObject::invokeMethod(source, "onRequestClearBuffer");
}

void LiveStreamSourceModel::EnableSourceRecording(LiveStreamSource *source, const QString &out_path)
{
    QMetaObject::invokeMethod(source, "onRequestSetRecordPath", Q_ARG(QString, out_path));
}

void LiveStreamSourceModel::DisableSourceRecording(LiveStreamSource *source)
{
    QMetaObject::invokeMethod(source, "onRequestSetRecordPath", Q_ARG(QString, QString()));
}

void LiveStreamSourceModel::LoadFromFile()
{
    QFile file("saved_sources.json");
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text))
        return;
    if (file.bytesAvailable() >= 0x1000)
        return;
    QJsonDocument json = QJsonDocument::fromJson(file.readAll());
    if (!json.isArray())
        return;
    QJsonArray json_array = json.array();

    for (const auto &item : json_array)
    {
        if (!item.isObject())
            continue;
        QJsonObject item_object = item.toObject();
        QString type = item_object.value("type").toString();
        if (type.isEmpty())
            continue;

        LiveStreamSource *source = nullptr;
        //TODO: Use QHash
        if (type == "bilibili")
            source = LiveStreamSourceBilibili::FromJson(item_object.value("data").toObject(), source_network_manager_);
        else if (type == "file")
            source = LiveStreamSourceFile::FromJson(item_object.value("data").toObject());
        if (source == nullptr)
            continue;

        QString name = item_object.value("name").toString();
        AddSource(source, name);
    }
}

void LiveStreamSourceModel::SaveToFile()
{
    QJsonArray json_array;
    for (const auto &p : sources_)
    {
        QJsonObject item;
        item["type"] = p.second->source()->SourceType();
        item["name"] = p.second->name();
        item["data"] = p.second->source()->ToJson();
        json_array.push_back(item);
    }

    QFile file("saved_sources.json");
    if (!file.open(QIODevice::WriteOnly | QIODevice::Text))
        return;
    file.write(QJsonDocument(json_array).toJson(QJsonDocument::Compact));
    file.close();
}
