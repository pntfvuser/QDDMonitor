#ifndef PCH_H
#define PCH_H

#include <cstdint>
#include <type_traits>

#include <atomic>
#include <chrono>
#include <memory>
#include <stdexcept>

#include <vector>
#include <string>

#include <QObject>
#include <QMetaType>
#include <QMetaMethod>
#include <QSharedPointer>

#include <QAbstractItemModel>
#include <QThread>
#include <QTimer>
#include <QScreen>

#include <QQuickWindow>
#include <QQuickItem>
#include <QtQuick/QQuickView>
#include <QSGTextureProvider>
#include <QSGSimpleTextureNode>

#include <QGuiApplication>
#include <QQmlApplicationEngine>

#ifdef _WIN32
#include <wrl/client.h>
using Microsoft::WRL::ComPtr;

#include <d3d11.h>
#include <dxgi1_2.h>
#include <DirectXMath.h>
#endif

extern "C"
{
#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
#include <libavutil/hwcontext.h>
#ifdef _WIN32
#include <libavutil/hwcontext_d3d11va.h>
#endif
#include <libavutil/pixdesc.h>
#include <libavutil/opt.h>
#include <libavutil/avassert.h>
#include <libswscale/swscale.h>
}

template <bool> struct PlaybackClockTypeHelper;
template <> struct PlaybackClockTypeHelper<true>
{
    using Clock = std::chrono::high_resolution_clock;
};
template <> struct PlaybackClockTypeHelper<false>
{
    using Clock = std::chrono::steady_clock;
};
using PlaybackClock = PlaybackClockTypeHelper<std::chrono::high_resolution_clock::is_steady>::Clock;

#endif // PCH_H
