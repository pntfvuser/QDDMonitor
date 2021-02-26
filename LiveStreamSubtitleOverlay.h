#ifndef LIVESTREAMVIEWSUBTITLEOVERLAY_H
#define LIVESTREAMVIEWSUBTITLEOVERLAY_H

#include "SubtitleFrame.h"

class LiveStreamSubtitleOverlay : public QQuickPaintedItem
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
        static constexpr int kProgressDen = 4096;

        QStaticText text;
        QColor color;
        SubtitleStyle style;
        int width = 0, slot = -1, progress_num = 0;
        bool occupies_slot = false;
    };

    Q_PROPERTY(qreal t READ t WRITE setT NOTIFY tChanged)
public:
    LiveStreamSubtitleOverlay(QQuickItem *parent = nullptr);

    qreal t() const { return t_; }
    void setT(qreal new_t) { if (t_ != new_t) { Update(new_t); emit tChanged(); } }

    void paint(QPainter *painter) override;
signals:
    void tChanged();
public slots:
    void onHeightChanged();
    void onNewSubtitleFrame(const QSharedPointer<SubtitleFrame> &subtitle_frame);
private:
    void Update(qreal t);

    static bool IsStyleFixedPosition(SubtitleStyle style);
    static unsigned char GetSlotBit(SubtitleStyle style);

    std::vector<SubtitleItem> active_subtitles_;
    QFont subtitle_font_;
    int subtitle_slot_height_;
    std::vector<unsigned char> subtitle_slot_busy_;
    qreal t_ = 0;
};

#endif // LIVESTREAMVIEWSUBTITLEOVERLAY_H
