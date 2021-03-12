#ifndef VIDEOFRAME_H
#define VIDEOFRAME_H

#include "AVObjectWrapper.h"

struct VideoFrame
{
    int64_t timestamp;
    PlaybackClock::time_point present_time;

    AVFrameObject frame;
};
Q_DECLARE_METATYPE(QSharedPointer<VideoFrame>);

#endif // VIDEOFRAME_H
