#ifndef VIDEOFRAME_H
#define VIDEOFRAME_H

//#include "AVObjectWrapper.h"

struct VideoFrame
{
    int64_t timestamp;
    PlaybackClock::time_point present_time;

    QSize size;
#ifdef _WIN32
    ComPtr<ID3D11Texture2D> texture = nullptr;
    bool is_rgbx = false;
#endif
};
Q_DECLARE_METATYPE(QSharedPointer<VideoFrame>);

#endif // VIDEOFRAME_H
