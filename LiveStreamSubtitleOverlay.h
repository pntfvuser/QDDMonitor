#ifndef LIVESTREAMVIEWSUBTITLEOVERLAY_H
#define LIVESTREAMVIEWSUBTITLEOVERLAY_H

#include "SubtitleFrame.h"

class LiveStreamSubtitleOverlay : public QQuickItem
{
    Q_OBJECT

    enum ItemType
    {
        ROW_NORMAL,
        ROW_TOP,
        ROW_BOTTOM,
        ROW_REVERSE,
    };
    static constexpr int ROW_TYPE_COUNT = ROW_REVERSE + 1;

    struct RowStatus
    {
        int status[4] = { -1, -1, -1, -1 };
    };

    struct SubtitleItem
    {
        static constexpr int kProgressDen = 8192;

        SubtitleItem() = default;
        SubtitleItem(QQmlContext *new_context, QQuickItem *new_item, ItemType new_style, int new_width) :visual_item_context(new_context), visual_item(new_item), style(new_style), width(new_width) {}

        QQmlContext *visual_item_context = nullptr;
        QQuickItem *visual_item = nullptr;
        ItemType style = ROW_NORMAL;
        int width = 0, row = -1, progress_num = 0;
    };

    Q_PROPERTY(qreal t READ t WRITE setT NOTIFY tChanged)
    Q_PROPERTY(QQmlComponent* textDelegate READ textDelegate WRITE setTextDelegate NOTIFY textDelegateChanged)
public:
    LiveStreamSubtitleOverlay(QQuickItem *parent = nullptr);

    qreal t() const { return t_; }
    void setT(qreal new_t) { if (t_ != new_t) { Update(new_t); emit tChanged(); } }

    QQmlComponent *textDelegate() const { return text_delegate_; }
    void setTextDelegate(QQmlComponent *new_text_delegate);
signals:
    void tChanged();
    void textDelegateChanged();
public slots:
    void onNewSubtitleFrame(const QSharedPointer<SubtitleFrame> &subtitle_frame);
private slots:
    void OnHeightChanged();
    void OnItemHeightChanged();
    void OnItemWidthChanged();
private:
    void UpdateHeight();
    void Update(qreal t);

    void UpdateItemY(const SubtitleItem &item);

    bool OccupiesRow(int index, const SubtitleItem &item);

    static bool IsStyleFixedPosition(ItemType style);
    static ItemType GetItemType(SubtitleStyle style);

    qreal t_ = 0;

    QQmlComponent *text_delegate_ = nullptr;
    QQmlContext *subtitle_row_height_context_ = nullptr;
    QQuickItem *subtitle_row_height_item_ = nullptr;
    int subtitle_row_height_ = 0, overlay_height_ = 0;

    std::vector<SubtitleItem> active_subtitles_;
    std::vector<RowStatus> subtitle_row_status_;
};

#endif // LIVESTREAMVIEWSUBTITLEOVERLAY_H
