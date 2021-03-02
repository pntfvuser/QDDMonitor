#include "pch.h"
#include "LiveStreamViewModel.h"

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

void LiveStreamViewModel::resetLayout(LiveStreamViewLayoutModel *layout_model)
{
    std::vector<LiveStreamViewInfo> new_view_info;
    for (const auto &item : layout_model->LayoutItems())
        new_view_info.push_back(LiveStreamViewInfo(item.row(), item.column(), item.rowSpan(), item.columnSpan(), item.isDummy() ? -2 : -1, nullptr, audio_out_));

    setRows(layout_model->rows());
    setColumns(layout_model->columns());
    beginResetModel();
    view_info_ = std::move(new_view_info);
    endResetModel();
}
