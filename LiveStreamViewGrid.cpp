#include "pch.h"
#include "LiveStreamViewGrid.h"

LiveStreamViewGrid::LiveStreamViewGrid(QQuickItem *parent)
    :QQuickItem(parent)
{
    row_height = height() / rows_;
    column_width = width() / columns_;
    connect(this, &QQuickItem::widthChanged, this, &LiveStreamViewGrid::OnWidthChanged);
    connect(this, &QQuickItem::heightChanged, this, &LiveStreamViewGrid::OnHeightChanged);
}

void LiveStreamViewGrid::setRows(int rows)
{
    if (rows_ != rows)
    {
        rows_ = rows;
        row_height = height() / rows_;
        if (!repositioning_)
        {
            repositioning_ = true;
            QTimer::singleShot(0, this, &LiveStreamViewGrid::Reposition);
        }
        emit rowsChanged();
    }
}

void LiveStreamViewGrid::setColumns(int columns)
{
    if (columns_ != columns)
    {
        columns_ = columns;
        column_width = width() / columns_;
        if (!repositioning_)
        {
            repositioning_ = true;
            QTimer::singleShot(0, this, &LiveStreamViewGrid::Reposition);
        }
        emit columnsChanged();
    }
}

void LiveStreamViewGrid::OnWidthChanged()
{
    column_width = width() / columns_;
    if (!repositioning_)
    {
        repositioning_ = true;
        QTimer::singleShot(0, this, &LiveStreamViewGrid::Reposition);
    }
}

void LiveStreamViewGrid::OnHeightChanged()
{
    row_height = height() / rows_;
    if (!repositioning_)
    {
        repositioning_ = true;
        QTimer::singleShot(0, this, &LiveStreamViewGrid::Reposition);
    }
}

void LiveStreamViewGrid::itemChange(QQuickItem::ItemChange change, const QQuickItem::ItemChangeData &value)
{
    if (change == ItemChildAddedChange)
    {
        AddChild(value.item);
        if (!repositioning_)
        {
            RepositionChild(row_height, column_width, value.item);
        }
    }
    else if (change == ItemChildRemovedChange)
    {
        RemoveChild(value.item);
    }

    QQuickItem::itemChange(change, value);
}

void LiveStreamViewGrid::AddChild(QQuickItem *child)
{
    LiveStreamViewGridAttachedType *attached = qobject_cast<LiveStreamViewGridAttachedType *>(qmlAttachedPropertiesObject<LiveStreamViewGrid>(child));
    if (!attached)
        return;
    auto reposition_child_callback = [this, child]()
    {
        if (!repositioning_)
        {
            RepositionChild(row_height, column_width, child);
        }
    };
    connect(attached, &LiveStreamViewGridAttachedType::rowChanged, this, reposition_child_callback);
    connect(attached, &LiveStreamViewGridAttachedType::columnChanged, this, reposition_child_callback);
    connect(attached, &LiveStreamViewGridAttachedType::rowSpanChanged, this, reposition_child_callback);
    connect(attached, &LiveStreamViewGridAttachedType::columnSpanChanged, this, reposition_child_callback);
}

void LiveStreamViewGrid::RemoveChild(QQuickItem *child)
{
    LiveStreamViewGridAttachedType *attached = qobject_cast<LiveStreamViewGridAttachedType *>(qmlAttachedPropertiesObject<LiveStreamViewGrid>(child));
    if (!attached)
        return;
    disconnect(attached, nullptr, this, nullptr);
}

void LiveStreamViewGrid::Reposition()
{
    repositioning_ = false;

    for (QQuickItem *child : childItems())
        RepositionChild(row_height, column_width, child);
}

void LiveStreamViewGrid::RepositionChild(qreal row_height, qreal column_width, QQuickItem *child)
{
    LiveStreamViewGridAttachedType *attached = qobject_cast<LiveStreamViewGridAttachedType *>(qmlAttachedPropertiesObject<LiveStreamViewGrid>(child));
    if (!attached || attached->row() < 0 || attached->column() < 0 || attached->rowSpan() <= 0 || attached->columnSpan() <= 0 || attached->row() + attached->rowSpan() > rows_ || attached->column() + attached->columnSpan() > columns_)
    {
        child->setX(0);
        child->setY(0);
        child->setWidth(0);
        child->setHeight(0);
        return;
    }
    child->setX(attached->column() * column_width);
    child->setY(attached->row() * row_height);
    child->setWidth(attached->columnSpan() * column_width);
    child->setHeight(attached->rowSpan() * row_height);
}
