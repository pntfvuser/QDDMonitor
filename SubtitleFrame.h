#ifndef SUBTITLEFRAME_H
#define SUBTITLEFRAME_H

enum class SubtitleStyle : unsigned char
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
