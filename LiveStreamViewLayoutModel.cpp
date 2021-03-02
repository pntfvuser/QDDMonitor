#include "pch.h"
#include "LiveStreamViewLayoutModel.h"

static inline constexpr bool overlapTest(int x1, int x1_span, int x2, int x2_span)
{
    if (x1 <= x2 && x2 < x1 + x1_span)
        return true;
    if (x1 < x2 + x2_span && x2 + x2_span <= x1 + x1_span)
        return true;
    return false;
}

LiveStreamViewLayoutModel::LiveStreamViewLayoutModel(QObject *parent)
    :QAbstractListModel(parent)
{
}

int LiveStreamViewLayoutModel::rowCount(const QModelIndex &) const
{
    return layout_items_.size();
}

QVariant LiveStreamViewLayoutModel::data(const QModelIndex &index, int role) const
{
    switch (role)
    {
    case Qt::DisplayRole:
        if (index.column() == 0 && index.row() >= 0 && index.row() < (int)layout_items_.size())
        {
            return QVariant::fromValue(layout_items_[index.row()]);
        }
        break;
    default:
        break;
    }

    return QVariant();
}

void LiveStreamViewLayoutModel::resetLayout(int rows, int column)
{
    setRows(rows);
    setColumns(column);
    beginResetModel();
    layout_items_.resize(1);
    layout_items_[0] = LiveStreamViewLayoutItem(0, 0, rows, column, true);
    endResetModel();
}

void LiveStreamViewLayoutModel::addLayoutItem(int row, int column, int row_span, int column_span)
{
    assert(!itemWillOverlap(row, column, row_span, column_span));
    if (!itemWillOverlap(row, column, row_span, column_span))
    {
        int new_row = (int)layout_items_.size();
        beginInsertRows(QModelIndex(), new_row, new_row);
        layout_items_.push_back(LiveStreamViewLayoutItem(row, column, row_span, column_span, false));
        endInsertRows();
    }
}

bool LiveStreamViewLayoutModel::itemWillOverlap(int row, int column, int row_span, int column_span) const
{
    for (size_t i = 1; i < layout_items_.size(); ++i)
    {
        const auto &item = layout_items_[i];
        if (overlapTest(row, row_span, item.row(), item.rowSpan()) && overlapTest(column, column_span, item.column(), item.columnSpan()))
            return true;
    }
    return false;
}
