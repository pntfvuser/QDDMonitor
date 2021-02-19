#ifndef AUDIOFRAME_H
#define AUDIOFRAME_H

#include "AVObjectWrapper.h"

struct AudioFrame
{
    int64_t timestamp;
    PlaybackClock::time_point present_time;

    int source;
    AVFrameObject frame;
};
Q_DECLARE_METATYPE(QSharedPointer<AudioFrame>);

#endif // AUDIOFRAME_H
