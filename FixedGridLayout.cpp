#include "pch.h"
#include "FixedGridLayout.h"

FixedGridLayout::FixedGridLayout(QQuickItem *parent)
    :QQuickItem(parent)
{
    row_height_ = height() / rows_;
    column_width_ = width() / columns_;
    connect(this, &QQuickItem::widthChanged, this, &FixedGridLayout::OnWidthChanged);
    connect(this, &QQuickItem::heightChanged, this, &FixedGridLayout::OnHeightChanged);
}

void FixedGridLayout::setRows(int rows)
{
    if (rows_ != rows)
    {
        rows_ = rows;
        row_height_ = height() / rows_;
        if (!repositioning_)
        {
            repositioning_ = true;
            QTimer::singleShot(0, this, &FixedGridLayout::Reposition);
        }
        emit rowsChanged();
    }
}

void FixedGridLayout::setColumns(int columns)
{
    if (columns_ != columns)
    {
        columns_ = columns;
        column_width_ = width() / columns_;
        if (!repositioning_)
        {
            repositioning_ = true;
            QTimer::singleShot(0, this, &FixedGridLayout::Reposition);
        }
        emit columnsChanged();
    }
}

void FixedGridLayout::OnWidthChanged()
{
    column_width_ = width() / columns_;
    if (!repositioning_)
    {
        repositioning_ = true;
        QTimer::singleShot(0, this, &FixedGridLayout::Reposition);
    }
}

void FixedGridLayout::OnHeightChanged()
{
    row_height_ = height() / rows_;
    if (!repositioning_)
    {
        repositioning_ = true;
        QTimer::singleShot(0, this, &FixedGridLayout::Reposition);
    }
}

void FixedGridLayout::itemChange(QQuickItem::ItemChange change, const QQuickItem::ItemChangeData &value)
{
    if (change == ItemChildAddedChange)
    {
        AddChild(value.item);
        if (!repositioning_)
        {
            RepositionChild(row_height_, column_width_, value.item);
        }
    }
    else if (change == ItemChildRemovedChange)
    {
        RemoveChild(value.item);
    }

    QQuickItem::itemChange(change, value);
}

void FixedGridLayout::AddChild(QQuickItem *child)
{
    FixedGridLayoutAttachedType *attached = qobject_cast<FixedGridLayoutAttachedType *>(qmlAttachedPropertiesObject<FixedGridLayout>(child));
    if (!attached)
        return;
    auto reposition_child_callback = [this, child]()
    {
        if (!repositioning_)
        {
            RepositionChild(row_height_, column_width_, child);
        }
    };
    connect(attached, &FixedGridLayoutAttachedType::rowChanged, this, reposition_child_callback);
    connect(attached, &FixedGridLayoutAttachedType::columnChanged, this, reposition_child_callback);
    connect(attached, &FixedGridLayoutAttachedType::rowSpanChanged, this, reposition_child_callback);
    connect(attached, &FixedGridLayoutAttachedType::columnSpanChanged, this, reposition_child_callback);
}

void FixedGridLayout::RemoveChild(QQuickItem *child)
{
    FixedGridLayoutAttachedType *attached = qobject_cast<FixedGridLayoutAttachedType *>(qmlAttachedPropertiesObject<FixedGridLayout>(child));
    if (!attached)
        return;
    disconnect(attached, nullptr, this, nullptr);
}

void FixedGridLayout::Reposition()
{
    repositioning_ = false;

    for (QQuickItem *child : childItems())
        RepositionChild(row_height_, column_width_, child);
}

void FixedGridLayout::RepositionChild(qreal row_height, qreal column_width, QQuickItem *child)
{
    FixedGridLayoutAttachedType *attached = qobject_cast<FixedGridLayoutAttachedType *>(qmlAttachedPropertiesObject<FixedGridLayout>(child));
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
