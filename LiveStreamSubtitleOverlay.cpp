#include "pch.h"
#include "LiveStreamSubtitleOverlay.h"

#include "LiveStreamView.h"

static constexpr int kSubtitleRowSpacing = 5, kSubtitleSameRowSpacing = 10;

LiveStreamSubtitleOverlay::LiveStreamSubtitleOverlay(QQuickItem *parent)
    :QQuickItem(parent)
{
    connect(this, &QQuickItem::heightChanged, this, &LiveStreamSubtitleOverlay::OnHeightChanged);
}

void LiveStreamSubtitleOverlay::setTextDelegate(QQmlComponent *new_text_delegate)
{
    if (text_delegate_ != new_text_delegate)
    {
        if (subtitle_row_height_item_)
        {
            delete subtitle_row_height_item_;
            subtitle_row_height_item_ = nullptr;
        }
        if (subtitle_row_height_context_)
        {
            delete subtitle_row_height_context_;
            subtitle_row_height_context_ = nullptr;
        }
        text_delegate_ = new_text_delegate;

        if (text_delegate_)
        {
            subtitle_row_height_context_ = new QQmlContext(text_delegate_->creationContext(), this);
            if (subtitle_row_height_context_)
            {
                subtitle_row_height_context_->setContextProperty("subtitleText", "Ip");
                subtitle_row_height_context_->setContextProperty("subtitleColor", QColor(0xFF, 0xFF, 0xFF, 0x00));
                subtitle_row_height_item_ = qobject_cast<QQuickItem *>(text_delegate_->create(subtitle_row_height_context_));
                if (subtitle_row_height_item_)
                {
                    subtitle_row_height_item_->setParent(this);
                    subtitle_row_height_item_->setParentItem(this);
                    subtitle_row_height_item_->setEnabled(false);
                    subtitle_row_height_item_->setVisible(false);
                    connect(subtitle_row_height_item_, &QQuickItem::heightChanged, this, &LiveStreamSubtitleOverlay::OnItemHeightChanged);
                    connect(subtitle_row_height_item_, &QQuickItem::widthChanged, this, &LiveStreamSubtitleOverlay::OnItemWidthChanged);
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

    active_subtitles_.emplace_back(text_item_context, text_item, GetItemType(frame->style), (int)round(text_item->width()));
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
    int new_subtitle_row_height;
    if (!subtitle_row_height_item_)
    {
        new_subtitle_row_height = 0;
    }
    else
    {
        new_subtitle_row_height = (int)round(subtitle_row_height_item_->height());
    }
    if (subtitle_row_height_ != new_subtitle_row_height)
    {
        subtitle_row_height_ = new_subtitle_row_height;
        UpdateHeight();
    }
}

void LiveStreamSubtitleOverlay::OnItemWidthChanged()
{
    for (SubtitleItem &item : active_subtitles_)
        item.width = (int)round(item.visual_item->width());
}

void LiveStreamSubtitleOverlay::UpdateHeight()
{
    if (overlay_height_ <= kSubtitleRowSpacing || subtitle_row_height_ <= 0)
    {
        subtitle_row_status_.clear();
        return;
    }

    subtitle_row_status_.resize((int)((overlay_height_ - kSubtitleRowSpacing) / (subtitle_row_height_ + kSubtitleRowSpacing)));

    for (SubtitleItem &item : active_subtitles_)
    {
        if (item.row != -1)
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
    int row_count = (int)subtitle_row_status_.size();
    for (int item_index = 0; item_index < (int)active_subtitles_.size(); ++item_index)
    {
        auto &item = active_subtitles_[item_index];
        if (item.row == -1)
        {
            for (int row = 0; row < row_count; ++row)
            {
                bool row_available = false;
                if (subtitle_row_status_[row].status[item.style] == -1)
                {
                    row_available = true;
                }
                else if (!IsStyleFixedPosition(item.style))
                {
                    int item_in_row_index = subtitle_row_status_[row].status[item.style];
                    Q_ASSERT(item_in_row_index < (int)active_subtitles_.size());
                    auto &item_in_row = active_subtitles_[item_in_row_index];
                    if (item_in_row.progress_num > (item_in_row.width + kSubtitleSameRowSpacing) * SubtitleItem::kProgressDen)
                        if ((long long)((item_in_row.width + overlay_width) * SubtitleItem::kProgressDen - item_in_row.progress_num) * (item.width + 800) < (long long)(overlay_width * SubtitleItem::kProgressDen) * (item_in_row.width + 800))
                            row_available = true;
                }

                if (row_available)
                {
                    subtitle_row_status_[row].status[item.style] = item_index;
                    item.row = row;
                    UpdateItemY(item);
                    item.visual_item->setEnabled(true);
                    item.visual_item->setVisible(true);
                    break;
                }
            }
        }
        if (item.row != -1)
        {
            int item_width = item.width;
            if (IsStyleFixedPosition(item.style)) //Fixed position style, progress_num is ms
            {
                item.progress_num += t_diff;
                constexpr int item_max_progress = 8000;
                if (item.progress_num > item_max_progress)
                {
                    if (OccupiesRow(item_index, item))
                        subtitle_row_status_[item.row].status[item.style] = -1;
                    item.row = -1;
                }
            }
            else //Variable position style, progress_num/kProgressDen is position
            {
                item.progress_num += (item_width + 800) * t_diff;
                int item_max_progress = item_width + overlay_width;
                if (item.progress_num > item_max_progress * SubtitleItem::kProgressDen)
                {
                    if (OccupiesRow(item_index, item))
                        subtitle_row_status_[item.row].status[item.style] = -1;
                    item.row = -1;
                }
            }

            qreal x;

            if (item.style == ROW_NORMAL)
                x = overlay_width - (qreal)item.progress_num / SubtitleItem::kProgressDen;
            else if (item.style == ROW_REVERSE)
                x = (qreal)item.progress_num / SubtitleItem::kProgressDen - item_width;
            else
                x = (qreal)(overlay_width - item_width) / 2;

            item.visual_item->setX(x);
        }
    }

    for (int i = 0; i < (int)active_subtitles_.size(); ++i)
    {
        auto &item = active_subtitles_[i];
        if (item.row == -1)
        {
            QQuickItem *visual_item = item.visual_item;
            QQmlContext *visual_item_context = item.visual_item_context;

            active_subtitles_.erase(active_subtitles_.begin() + i);
            for (int j = 0; j < (int)subtitle_row_status_.size(); ++j)
            {
                for (int k = 0; k < ROW_TYPE_COUNT; ++k)
                {
                    if (subtitle_row_status_[j].status[k] > i)
                        subtitle_row_status_[j].status[k] -= 1;
                    else if (subtitle_row_status_[j].status[k] == i)
                        subtitle_row_status_[j].status[k] = -1;
                }
            }

            if (visual_item)
                delete visual_item;
            if (visual_item_context)
                delete visual_item_context;
        }
    }


#ifdef _DEBUG
    for (int j = 0; j < (int)subtitle_row_status_.size(); ++j)
        for (int k = 0; k < ROW_TYPE_COUNT; ++k)
            if (subtitle_row_status_[j].status[k] != -1 && (subtitle_row_status_[j].status[k] >= (int)active_subtitles_.size() || active_subtitles_[subtitle_row_status_[j].status[k]].row != j))
                Q_ASSERT(false);
#endif
}

void LiveStreamSubtitleOverlay::UpdateItemY(const SubtitleItem &item)
{
    int y;
    if (item.style == ROW_BOTTOM)
        y = overlay_height_ - (item.row + 1) * (subtitle_row_height_ + kSubtitleRowSpacing);
    else
        y = item.row * (subtitle_row_height_ + kSubtitleRowSpacing) + kSubtitleRowSpacing;
    item.visual_item->setY(y);
}

bool LiveStreamSubtitleOverlay::OccupiesRow(int index, const SubtitleItem &item)
{
    Q_ASSERT(index >= 0 && index < (int)active_subtitles_.size());
    return item.row >= 0 && item.row < (int)subtitle_row_status_.size() && subtitle_row_status_[item.row].status[item.style] == index;
}

bool LiveStreamSubtitleOverlay::IsStyleFixedPosition(ItemType style)
{
    switch (style)
    {
    case ROW_TOP:
    case ROW_BOTTOM:
        return true;
    case ROW_NORMAL:
    case ROW_REVERSE:
        return false;
    }
    return false;
}

LiveStreamSubtitleOverlay::ItemType LiveStreamSubtitleOverlay::GetItemType(SubtitleStyle style)
{
    switch (style)
    {
    case SubtitleStyle::NORMAL:
        return ROW_NORMAL;
    case SubtitleStyle::TOP:
        return ROW_TOP;
    case SubtitleStyle::BOTTOM:
        return ROW_BOTTOM;
    case SubtitleStyle::REVERSE:
        return ROW_REVERSE;
    default:
        return ROW_NORMAL;
    }
}
