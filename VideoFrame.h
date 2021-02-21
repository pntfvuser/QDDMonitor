#ifndef VIDEOFRAME_H
#define VIDEOFRAME_H

//#include "AVObjectWrapper.h"

struct VideoFrame
{
    int64_t timestamp;
    PlaybackClock::time_point present_time;

#ifdef _WIN32
    ComPtr<ID3D11Texture2D> texture = nullptr;
#endif
    enum
    {
        RGBX,
        YUVJ444P,
        NV12,
    } texture_format;
    QSize size;
};
Q_DECLARE_METATYPE(QSharedPointer<VideoFrame>);

#endif // VIDEOFRAME_H
