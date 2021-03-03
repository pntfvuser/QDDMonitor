#include "pch.h"
#include "LiveStreamSourceModel.h"

#include "LiveStreamSource.h"
#include "LiveStreamSourceBilibili.h"

LiveStreamSourceModel::LiveStreamSourceModel(QObject *parent)
    :QAbstractListModel(parent)
{
    source_network_manager_ = new QNetworkAccessManager;
    source_network_manager_->moveToThread(&source_thread_);
    connect(&source_thread_, &QThread::finished, source_network_manager_, &QObject::deleteLater);
    source_thread_.start();

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
                return QVariant::fromValue(itr->second);
        }
        break;
    default:
        break;
    }

    return QVariant();
}

void LiveStreamSourceModel::addBilibiliSource(const QString &name, int room_display_id)
{
    AddSource(new LiveStreamSourceBilibili(room_display_id, source_network_manager_), name);
}

void LiveStreamSourceModel::setSourceOption(int id, const QString &option)
{
    auto itr = sources_.find(id);
    if (itr != sources_.end())
    {
        itr->second.setOption(option);
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

        emit deleteSource(itr->second.id());
        activated_sources_.erase(itr->second.id());
        itr->second.source()->deleteLater();
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
            emit deleteSource(itr->second.id());
            activated_sources_.erase(itr->second.id());
            itr->second.source()->deleteLater();
            sources_.erase(itr);
        }
    }
}

LiveStreamSource *LiveStreamSourceModel::ActivateAndGetSource(int source_id)
{
    if (activated_sources_.count(source_id) > 0)
        return nullptr;
    auto itr = sources_.find(source_id);
    if (itr == sources_.end())
        return nullptr;
    activated_sources_.emplace(source_id);

    const LiveStreamSourceInfo &source_info = itr->second;
    if (source_info.online() && !source_info.activated())
        ActivateSource(source_info.source(), source_info.effectiveOption());

    return itr->second.source();
}

void LiveStreamSourceModel::DeactivateSingleSource(int source_id)
{
    if (activated_sources_.erase(source_id) > 0)
    {
        auto itr = sources_.find(source_id);
        if (itr != sources_.end() && itr->second.activated())
            DeactivateSource(itr->second.source());
    }
}

void LiveStreamSourceModel::ActivateAndGetSources(std::vector<std::pair<int, LiveStreamSource *>> &sources)
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
            const LiveStreamSourceInfo &source_info = itr->second;
            p.second = source_info.source();

            auto itr_activated = activated_sources_.find(p.first);
            if (itr_activated == activated_sources_.end())
            {
                //Not previously marked as activated
                if (source_info.online() && !source_info.activated())
                {
                    //Not activated yet and online, activate
                    ActivateSource(source_info.source(), source_info.effectiveOption());
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
        if (itr != sources_.end() && itr->second.activated())
        {
            DeactivateSource(itr->second.source());
        }
    }
    activated_sources_ = std::move(new_activated_sources);
}

void LiveStreamSourceModel::DeactivateAllSource()
{
    for (int id : activated_sources_)
    {
        auto itr = sources_.find(id);
        if (itr != sources_.end() && itr->second.activated())
            DeactivateSource(itr->second.source());
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
    sources_.emplace(id, LiveStreamSourceInfo(id, source, name));

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
    itr->second.setActivated(true);

    int idx = FindSourceIndex(id);
    if (idx != -1)
        emit dataChanged(index(idx), index(idx));

    if (activated_sources_.count(id) == 0)
        DeactivateSource(itr->second.source());
}

void LiveStreamSourceModel::OnDeactivated(int id)
{
    auto itr = sources_.find(id);
    if (itr == sources_.end())
        return;
    itr->second.setActivated(false);

    int idx = FindSourceIndex(id);
    if (idx != -1)
        emit dataChanged(index(idx), index(idx));

    if (itr->second.online() && activated_sources_.count(id) > 0)
        ActivateSource(itr->second.source(), itr->second.effectiveOption());
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
        QTimer::singleShot(5 * 1000, this, &LiveStreamSourceModel::StartUpdateSources);
        return;
    }
    int id = sources_updating_[sources_updated_count_];
    auto itr = sources_.find(id);
    if (itr == sources_.end())
    {
        UpdateSingleSourceCanceled();
        return;
    }
    LiveStreamSource *source = itr->second.source();
    connect(source, &LiveStreamSource::infoUpdated, this, &LiveStreamSourceModel::UpdateSingleSourceDone);
    QMetaObject::invokeMethod(source, "onRequestUpdateInfo");
}

void LiveStreamSourceModel::UpdateSingleSourceDone(int status, const QString &description, const QList<QString> &options)
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
    LiveStreamSource *source = itr->second.source();
    if (sender() != source)
    {
        return;
    }
    disconnect(source, &LiveStreamSource::infoUpdated, this, &LiveStreamSourceModel::UpdateSingleSourceDone);

    LiveStreamSourceInfo &source_info = itr->second;
    source_info.setDescription(description);
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
            //Emit dataChanged
            emit dataChanged(index(i), index(i));

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
            //Emit dataChanged
            emit dataChanged(index(i), index(i));

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
    else
    {
        int idx = FindSourceIndex(id);
        if (idx != -1)
            emit dataChanged(index(idx), index(idx));
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
