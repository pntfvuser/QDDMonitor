#include "pch.h"
#include "LiveStreamViewModel.h"

#include "LiveStreamSourceModel.h"
#include "LiveStreamViewLayoutModel.h"
#include "AudioOutput.h"

LiveStreamViewModel::LiveStreamViewModel(QObject *parent)
    :QAbstractListModel(parent)
{
    audio_out_ = new AudioOutput;
    audio_out_->moveToThread(&audio_thread_);
    connect(&audio_thread_, &QThread::finished, audio_out_, &QObject::deleteLater);
    audio_thread_.start();
}

LiveStreamViewModel::~LiveStreamViewModel()
{
    audio_thread_.exit();
    audio_thread_.wait();
}

int LiveStreamViewModel::rowCount(const QModelIndex &) const
{
    return view_info_.size();
}

QVariant LiveStreamViewModel::data(const QModelIndex &index, int role) const
{
    switch (role)
    {
    case Qt::DisplayRole:
        if (index.column() == 0 && index.row() >= 0 && index.row() < (int)view_info_.size())
            return QVariant::fromValue(view_info_[index.row()]);
        break;
    default:
        break;
    }

    return QVariant();
}

void LiveStreamViewModel::setSourceModel(LiveStreamSourceModel *new_source_model)
{
    if (source_model_ != new_source_model)
    {
        if (source_model_)
        {
            disconnect(source_model_, &LiveStreamSourceModel::deleteSource, this, &LiveStreamViewModel::OnDeleteSource);
        }
        source_model_ = new_source_model;
        if (source_model_)
        {
            connect(source_model_, &LiveStreamSourceModel::deleteSource, this, &LiveStreamViewModel::OnDeleteSource); //Should be direct
        }
        emit sourceModelChanged();
    }
}

void LiveStreamViewModel::resetLayout(LiveStreamViewLayoutModel *layout_model)
{
    if (!layout_model)
        return;
    std::vector<LiveStreamViewInfo> new_view_info;
    for (const auto &item : layout_model->LayoutItems())
        new_view_info.push_back(LiveStreamViewInfo(item.row(), item.column(), item.rowSpan(), item.columnSpan(), -1, nullptr, audio_out_));

    if (Q_LIKELY(source_model_))
        source_model_->DeactivateAllSource();

    setRows(layout_model->rows());
    setColumns(layout_model->columns());
    beginResetModel();
    view_info_ = std::move(new_view_info);
    endResetModel();
}

void LiveStreamViewModel::setSource(int index_row, int source_id)
{
    if (index_row >= 0 && index_row < (int)view_info_.size() && Q_LIKELY(source_model_))
    {
        LiveStreamViewInfo &view_info = view_info_[index_row];

        LiveStreamSource *source = source_model_->ActivateAndGetSource(source_id);
        if (source)
        {
            if (view_info.sourceId() != -1)
                source_model_->DeactivateSingleSource(view_info.sourceId());
            view_info.setSource(index_row, source);
            emit dataChanged(index(index_row), index(index_row));
        }
    }
}

void LiveStreamViewModel::OnDeleteSource(int source_id)
{
    for (int i = 0; i < (int)view_info_.size(); ++i)
    {
        auto &view_info = view_info_[i];
        if (view_info.sourceId() == source_id)
        {
            view_info.setSource(-1, nullptr);
            emit dataChanged(index(i), index(i));
        }
    }
}
