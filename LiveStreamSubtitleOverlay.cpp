#include "pch.h"
#include "LiveStreamSubtitleOverlay.h"

#include "LiveStreamView.h"

static constexpr int kSubtitleSameSlotMargin = 10;

LiveStreamSubtitleOverlay::LiveStreamSubtitleOverlay(QQuickItem *parent)
    :QQuickPaintedItem(parent), subtitle_font_(QGuiApplication::font())
{
    connect(this, &QQuickItem::heightChanged, this, &LiveStreamSubtitleOverlay::onHeightChanged);

    subtitle_font_.setPixelSize(20);
    QStaticText height_test_text("Ip");
    height_test_text.prepare(QTransform(), subtitle_font_);
    subtitle_slot_height_ = round(height_test_text.size().height());
    if (height() >= subtitle_slot_height_)
        subtitle_slot_busy_.resize((int)(height() / subtitle_slot_height_), 0);
}

void LiveStreamSubtitleOverlay::paint(QPainter *painter)
{
    painter->setFont(subtitle_font_);
    painter->setClipRect(0, 0, width(), height());
    painter->setClipping(true);

    int overlay_height = (int)height();
    int overlay_width = (int)width();
    for (const SubtitleItem &item : active_subtitles_)
    {
        if (item.slot != -1)
        {
            int item_width = item.width;
            int x, y;

            if (item.style == SubtitleStyle::BOTTOM)
                y = overlay_height - (item.slot + 1) * subtitle_slot_height_;
            else
                y = item.slot * subtitle_slot_height_;

            if (item.style == SubtitleStyle::NORMAL)
                x = overlay_width - item.progress_num / SubtitleItem::kProgressDen;
            else if (item.style == SubtitleStyle::REVERSE)
                x = item.progress_num / SubtitleItem::kProgressDen - item_width;
            else
                x = (overlay_width - item_width) / 2;

            painter->setPen(item.color);
            painter->drawStaticText(x, y, item.text);
        }
    }
}

void LiveStreamSubtitleOverlay::onHeightChanged()
{
    size_t old_slot_count = subtitle_slot_busy_.size();
    subtitle_slot_busy_.resize((int)(height() / subtitle_slot_height_));
    if (subtitle_slot_busy_.size() < old_slot_count)
    {
        for (SubtitleItem &item : active_subtitles_)
        {
            if (item.occupies_slot && item.slot >= (int)subtitle_slot_busy_.size())
                item.occupies_slot = false;
        }
    }
}

void LiveStreamSubtitleOverlay::onNewSubtitleFrame(const QSharedPointer<SubtitleFrame> &frame)
{
    active_subtitles_.emplace_back();
    SubtitleItem &item = active_subtitles_.back();
    item.text.setText(frame->content);
    item.text.prepare(QTransform(), subtitle_font_);
    item.color = frame->color;
    item.style = frame->style;
    item.width = (int)round(item.text.size().width());

    update();
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
        }
    }

    for (auto itr = active_subtitles_.begin(); itr != active_subtitles_.end();)
    {
        if (itr->slot == -1)
            itr = active_subtitles_.erase(itr);
        else
            ++itr;
    }

    update();
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
        return SLOT_BUSY_NORMAL;
    case SubtitleStyle::TOP:
        return SLOT_BUSY_TOP;
    case SubtitleStyle::BOTTOM:
        return SLOT_BUSY_BOTTOM;
    case SubtitleStyle::REVERSE:
        return SLOT_BUSY_REVERSE;
    }
    return 0;
}
