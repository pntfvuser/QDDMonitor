#include "pch.h"
#include "LiveStreamViewModel.h"

LiveStreamViewModel::LiveStreamViewModel(QObject *parent)
    :QAbstractListModel(parent)
{

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
        if (index.column() == 1 && index.row() >= 0 && index.row() < (int)view_info_.size())
            return QVariant::fromValue(view_info_[index.row()]);
        break;
    default:
        break;
    }

    return QVariant();
}
