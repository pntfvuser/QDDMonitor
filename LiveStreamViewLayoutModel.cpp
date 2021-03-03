#include "pch.h"
#include "LiveStreamViewLayoutModel.h"

static inline constexpr bool intersectTest(int x1, int x1_span, int x2, int x2_span)
{
    return x1 < x2 + x2_span && x2 < x1 + x1_span;
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

void LiveStreamViewLayoutModel::resetLayout(int rows, int columns)
{
    setRows(rows);
    setColumns(columns);
    beginResetModel();
    layout_items_.clear();
    endResetModel();
}

void LiveStreamViewLayoutModel::addLayoutItem(int row, int column, int row_span, int column_span)
{
    if (row < 0 || row + row_span > rows() || row_span <= 0)
        return;
    if (column < 0 || column + column_span > columns() || column_span <= 0)
        return;
    if (itemWillIntersect(row, column, row_span, column_span))
        return;
    int new_row = (int)layout_items_.size();
    beginInsertRows(QModelIndex(), new_row, new_row);
    layout_items_.push_back(LiveStreamViewLayoutItem(row, column, row_span, column_span));
    endInsertRows();
}

bool LiveStreamViewLayoutModel::itemWillIntersect(int row, int column, int row_span, int column_span) const
{
    for (size_t i = 0; i < layout_items_.size(); ++i)
    {
        const auto &item = layout_items_[i];
        if (intersectTest(row, row_span, item.row(), item.rowSpan()) && intersectTest(column, column_span, item.column(), item.columnSpan()))
            return true;
    }
    return false;
}
