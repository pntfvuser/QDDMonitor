#include "pch.h"
#include "LiveStreamSourceModel.h"

#include "LiveStreamSource.h"
#include "LiveStreamSourceBilibili.h"

LiveStreamSourceModel::LiveStreamSourceModel(QObject *parent)
    :QAbstractListModel(parent)
{
    source_thread_.start();
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
        if (index.column() == 1 && index.row() >= 0 && index.row() < (int)sources_index_.size())
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
    addSource(name, new LiveStreamSourceBilibili(room_display_id));
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
                endRemoveRows();
            }
        }

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
        endRemoveRows();

        auto itr = sources_.find(id);
        if (itr != sources_.end())
        {
            itr->second.source()->deleteLater();
            sources_.erase(id);
        }
    }
}

void LiveStreamSourceModel::addSource(const QString &name, LiveStreamSource *source)
{
    source->moveToThread(&source_thread_);
    QObject::connect(&source_thread_, &QThread::finished, source, &QObject::deleteLater);
    int id = next_id_++;
    sources_.emplace(id, LiveStreamSourceInfo(id, name, source));

    int index = (int)sources_index_.size();
    beginInsertRows(QModelIndex(), index, index);
    sources_index_.push_back(id);
    endInsertRows();
}
