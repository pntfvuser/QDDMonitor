#include "pch.h"
#include "LiveStreamSubtitleOverlay.h"

#include "LiveStreamView.h"

static constexpr int kSubtitleSameSlotMargin = 10;
static constexpr int kSubtitleDifferentSlotMargin = 5;

LiveStreamSubtitleOverlay::LiveStreamSubtitleOverlay(QQuickItem *parent)
    :QQuickItem(parent)
{
    connect(this, &QQuickItem::heightChanged, this, &LiveStreamSubtitleOverlay::OnHeightChanged);
}

void LiveStreamSubtitleOverlay::setTextDelegate(QQmlComponent *new_text_delegate)
{
    if (text_delegate_ != new_text_delegate)
    {
        if (subtitle_slot_height_item_)
        {
            delete subtitle_slot_height_item_;
            subtitle_slot_height_item_ = nullptr;
        }
        if (subtitle_slot_height_context_)
        {
            delete subtitle_slot_height_context_;
            subtitle_slot_height_context_ = nullptr;
        }
        text_delegate_ = new_text_delegate;

        if (text_delegate_)
        {
            subtitle_slot_height_context_ = new QQmlContext(text_delegate_->creationContext(), this);
            if (subtitle_slot_height_context_)
            {
                subtitle_slot_height_context_->setContextProperty("subtitleText", "Ip");
                subtitle_slot_height_context_->setContextProperty("subtitleColor", QColor(0xFF, 0xFF, 0xFF, 0x00));
                subtitle_slot_height_item_ = qobject_cast<QQuickItem *>(text_delegate_->create(subtitle_slot_height_context_));
                if (subtitle_slot_height_item_)
                {
                    subtitle_slot_height_item_->setParent(this);
                    subtitle_slot_height_item_->setParentItem(this);
                    subtitle_slot_height_item_->setEnabled(false);
                    subtitle_slot_height_item_->setVisible(false);
                    connect(subtitle_slot_height_item_, &QQuickItem::heightChanged, this, &LiveStreamSubtitleOverlay::OnItemHeightChanged);
                    connect(subtitle_slot_height_item_, &QQuickItem::widthChanged, this, &LiveStreamSubtitleOverlay::OnItemWidthChanged);
                }
            }
            OnItemHeightChanged();
        }

        emit textDelegateChanged();
    }
}

void LiveStreamSubtitleOverlay::onNewSubtitleFrame(const QSharedPointer<SubtitleFrame> &frame)
{
    if (!text_delegate_)
        return;

    QQmlContext *text_item_context = new QQmlContext(text_delegate_->creationContext(), this);
    if (!text_item_context)
        return;
    text_item_context->setContextProperty("subtitleText", frame->content);
    text_item_context->setContextProperty("subtitleColor", frame->color);

    QQuickItem *text_item = qobject_cast<QQuickItem *>(text_delegate_->create(text_item_context));
    if (!text_item)
    {
        delete text_item_context;
        return;
    }
    text_item->setParent(this);
    text_item->setParentItem(this);
    text_item->setEnabled(false);
    text_item->setVisible(false);

    active_subtitles_.emplace_back();
    SubtitleItem &item = active_subtitles_.back();

    item.item = text_item;
    item.item_context = text_item_context;
    item.style = frame->style;
    item.width = (int)round(text_item->width());
}

void LiveStreamSubtitleOverlay::OnHeightChanged()
{
    int new_overlay_height = (int)round(height());
    if (overlay_height_ != new_overlay_height)
    {
        overlay_height_ = new_overlay_height;
        UpdateHeight();
    }
}

void LiveStreamSubtitleOverlay::OnItemHeightChanged()
{
    int new_subtitle_slot_height;
    if (!subtitle_slot_height_item_)
    {
        new_subtitle_slot_height = 0;
    }
    else
    {
        new_subtitle_slot_height = (int)round(subtitle_slot_height_item_->height());
    }
    if (subtitle_slot_height_ != new_subtitle_slot_height)
    {
        subtitle_slot_height_ = new_subtitle_slot_height;
        UpdateHeight();
    }
}

void LiveStreamSubtitleOverlay::OnItemWidthChanged()
{
    for (SubtitleItem &item : active_subtitles_)
        item.width = (int)round(item.item->width());
}

void LiveStreamSubtitleOverlay::UpdateHeight()
{
    if (overlay_height_ <= kSubtitleDifferentSlotMargin || subtitle_slot_height_ <= 0)
    {
        subtitle_slot_busy_.clear();
        return;
    }

    size_t old_slot_count = subtitle_slot_busy_.size();
    subtitle_slot_busy_.resize((int)((overlay_height_ - kSubtitleDifferentSlotMargin) / (subtitle_slot_height_ + kSubtitleDifferentSlotMargin)), 0);
    if (subtitle_slot_busy_.size() < old_slot_count)
    {
        for (SubtitleItem &item : active_subtitles_)
        {
            if (item.occupies_slot && item.slot >= (int)subtitle_slot_busy_.size())
                item.occupies_slot = false;
        }
    }

    for (SubtitleItem &item : active_subtitles_)
    {
        if (item.slot != -1)
        {
            UpdateItemY(item);
        }
    }
}

void LiveStreamSubtitleOverlay::Update(qreal t)
{
    int t_diff = (int)round(t - t_);
    while (t_diff < 0)
        t_diff += LiveStreamView::kAnimationTimeSourcePeriod;
    t_ = t;

    int overlay_width = (int)width();
    int slot_count = (int)subtitle_slot_busy_.size();
    for (SubtitleItem &item : active_subtitles_)
    {
        if (item.slot == -1)
        {
            unsigned char slot_bit = GetSlotBit(item.style);
            for (int i = 0; i < slot_count; ++i)
            {
                if ((subtitle_slot_busy_[i] & slot_bit) == 0)
                {
                    subtitle_slot_busy_[i] |= slot_bit;
                    item.slot = i;
                    item.occupies_slot = true;
                    item.item->setEnabled(true);
                    item.item->setVisible(true);
                    UpdateItemY(item);
                    break;
                }
            }
        }
        if (item.slot != -1)
        {
            int item_width = item.width;
            if (IsStyleFixedPosition(item.style)) //Fixed position style, progress_num is ms
            {
                item.progress_num += t_diff;
                constexpr int item_max_progress = 8000;
                if (item.progress_num > item_max_progress)
                {
                    if (item.occupies_slot && item.slot < slot_count)
                        subtitle_slot_busy_[item.slot] &= ~GetSlotBit(item.style);
                    item.slot = -1;
                }
            }
            else //Variable position style, progress_num/kProgressDen is position
            {
                item.progress_num += (item_width + 800) * t_diff;
                int item_max_progress = item_width + overlay_width;
                if (item.progress_num > item_max_progress * SubtitleItem::kProgressDen)
                {
                    if (item.occupies_slot && item.slot < slot_count)
                        subtitle_slot_busy_[item.slot] &= ~GetSlotBit(item.style);
                    item.slot = -1;
                }
                else if (item.occupies_slot && item.progress_num > (item_width + kSubtitleSameSlotMargin) * SubtitleItem::kProgressDen)
                {
                    if (item.slot < slot_count)
                        subtitle_slot_busy_[item.slot] &= ~GetSlotBit(item.style);
                    item.occupies_slot = false;
                }
            }

            qreal x;

            if (item.style == SubtitleStyle::NORMAL)
                x = overlay_width - (qreal)item.progress_num / SubtitleItem::kProgressDen;
            else if (item.style == SubtitleStyle::REVERSE)
                x = (qreal)item.progress_num / SubtitleItem::kProgressDen - item_width;
            else
                x = (qreal)(overlay_width - item_width) / 2;

            item.item->setX(x);
        }
    }

    for (auto itr = active_subtitles_.begin(); itr != active_subtitles_.end();)
    {
        if (itr->slot == -1)
        {
            if (itr->item)
            {
                delete itr->item;
                itr->item = nullptr;
            }
            if (itr->item_context)
            {
                delete itr->item_context;
                itr->item = nullptr;
            }
            itr = active_subtitles_.erase(itr);
        }
        else
        {
            ++itr;
        }
    }
}

void LiveStreamSubtitleOverlay::UpdateItemY(const LiveStreamSubtitleOverlay::SubtitleItem &item)
{
    int y;
    if (item.style == SubtitleStyle::BOTTOM)
        y = overlay_height_ - (item.slot + 1) * (subtitle_slot_height_ + kSubtitleDifferentSlotMargin);
    else
        y = item.slot * (subtitle_slot_height_ + kSubtitleDifferentSlotMargin) + kSubtitleDifferentSlotMargin;
    item.item->setY(y);
}

bool LiveStreamSubtitleOverlay::IsStyleFixedPosition(SubtitleStyle style)
{
    switch (style)
    {
    case SubtitleStyle::TOP:
    case SubtitleStyle::BOTTOM:
        return true;
    case SubtitleStyle::NORMAL:
    case SubtitleStyle::REVERSE:
        return false;
    }
    return false;
}

unsigned char LiveStreamSubtitleOverlay::GetSlotBit(SubtitleStyle style)
{
    switch (style)
    {
    case SubtitleStyle::NORMAL:
        return ROW_BUSY_NORMAL;
    case SubtitleStyle::TOP:
        return ROW_BUSY_TOP;
    case SubtitleStyle::BOTTOM:
        return ROW_BUSY_BOTTOM;
    case SubtitleStyle::REVERSE:
        return ROW_BUSY_REVERSE;
    }
    return 0;
}
