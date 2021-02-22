#ifndef SUBTITLEFRAME_H
#define SUBTITLEFRAME_H

enum class SubtitleStyle
{
    NORMAL,
    TOP,
    BOTTOM,
    REVERSE,
};

struct SubtitleFrame
{
    QString content;
    QColor color;
    SubtitleStyle style;
};
Q_DECLARE_METATYPE(QSharedPointer<SubtitleFrame>);

#endif // SUBTITLEFRAME_H
