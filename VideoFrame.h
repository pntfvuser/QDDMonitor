#ifndef VIDEOFRAME_H
#define VIDEOFRAME_H

#include "AVObjectWrapper.h"

struct VideoFrame
{
    //AVFrameObject frame;
    QSize frame_size;
    PlaybackClock::time_point frame_time;

#ifdef _WIN32
    ComPtr<ID3D11Texture2D> texture = nullptr;
    bool is_rgbx = false;
#endif
};
Q_DECLARE_METATYPE(QSharedPointer<VideoFrame>);

#endif // VIDEOFRAME_H
