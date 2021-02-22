#ifndef LIVESTREAMVIEWSUBTITLEOVERLAY_H
#define LIVESTREAMVIEWSUBTITLEOVERLAY_H

#include "SubtitleFrame.h"

class LiveStreamViewSubtitleOverlay : public QQuickPaintedItem
{
    Q_OBJECT

    enum : unsigned char
    {
        SLOT_BUSY_NORMAL = 0x01,
        SLOT_BUSY_TOP = 0x02,
        SLOT_BUSY_BOTTOM = 0x04,
        SLOT_BUSY_REVERSE = 0x08,
    };

    struct SubtitleItem
    {
        QStaticText text;
        QColor color;
        SubtitleStyle style;
        int slot = -1, progress = 0;
        bool occupies_slot = false;
    };
public:
    LiveStreamViewSubtitleOverlay(QQuickItem *parent = nullptr);

    void AddSubtitle(const QSharedPointer<SubtitleFrame> &frame);
    void Update(qreal t);

    void paint(QPainter *painter) override;
signals:

private slots:
    void OnParentWidthChanged() { setWidth(parentItem()->width()); }
    void OnParentHeightChanged();
private:
    static unsigned char GetSlotBit(SubtitleStyle style);

    std::vector<SubtitleItem> active_subtitles_;
    QFont subtitle_font_;
    int subtitle_slot_height_;
    std::vector<unsigned char> subtitle_slot_busy_;
    qreal t_ = 0;
};

#endif // LIVESTREAMVIEWSUBTITLEOVERLAY_H
