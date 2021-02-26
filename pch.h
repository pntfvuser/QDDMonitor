#ifndef PCH_H
#define PCH_H

#include <cmath>
#include <cstdint>
#include <cstdlib>
#include <type_traits>

#include <atomic>
#include <chrono>
#include <memory>
#include <stdexcept>
using namespace std::chrono_literals;

#include <vector>
#include <unordered_map>
#include <string>

#include <QObject>
#include <QMetaType>
#include <QMetaMethod>

#include <QtEndian>
#include <QWaitCondition>
#include <QIODevice>
#include <QHash>
#include <QMutex>
#include <QMutexLocker>
#include <QThread>
#include <QSharedPointer>
#include <QUrl>
#include <QUrlQuery>

#include <QAbstractListModel>
#include <QFile>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QLoggingCategory>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QTimer>
#include <QPainter>
#include <QScreen>
#include <QStaticText>

#include <QQuickWindow>
#include <QQuickItem>
#include <QQuickPaintedItem>
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

#include <AL/al.h>
#include <AL/alc.h>
#include <AL/alext.h>

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
#include <libswresample/swresample.h>
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
