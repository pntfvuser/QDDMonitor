#ifndef LIVESTREAMVIEWSUBTITLEOVERLAY_H
#define LIVESTREAMVIEWSUBTITLEOVERLAY_H

#include "SubtitleFrame.h"

class LiveStreamSubtitleOverlay : public QQuickItem
{
    Q_OBJECT

    enum : unsigned char
    {
        ROW_BUSY_NORMAL = 0x01,
        ROW_BUSY_TOP = 0x02,
        ROW_BUSY_BOTTOM = 0x04,
        ROW_BUSY_REVERSE = 0x08,
    };

    struct SubtitleItem
    {
        static constexpr int kProgressDen = 8192;

        QQmlContext *item_context = nullptr;
        QQuickItem *item = nullptr;
        SubtitleStyle style;
        int width = 0, slot = -1, progress_num = 0;
        bool occupies_slot = false;
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

    static bool IsStyleFixedPosition(SubtitleStyle style);
    static unsigned char GetSlotBit(SubtitleStyle style);

    qreal t_ = 0;

    QQmlComponent *text_delegate_ = nullptr;
    QQmlContext *subtitle_slot_height_context_ = nullptr;
    QQuickItem *subtitle_slot_height_item_ = nullptr;
    int subtitle_slot_height_ = 0, overlay_height_ = 0;

    std::vector<SubtitleItem> active_subtitles_;
    std::vector<unsigned char> subtitle_slot_busy_;
};

#endif // LIVESTREAMVIEWSUBTITLEOVERLAY_H
