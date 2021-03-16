#ifndef PCH_H
#define PCH_H

#include <cmath>
#include <cstdint>
#include <cstdlib>
#include <limits>
#include <type_traits>

#include <atomic>
#include <chrono>
#include <memory>
#include <stdexcept>
using namespace std::chrono_literals;

#include <vector>
#include <unordered_set>
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
#include <QDateTime>
#include <QDir>
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
#include <QWebSocket>

#include <QOpenGLFunctions>
#include <QOpenGLBuffer>
#include <QOpenGLTexture>
#include <QOpenGLPixelTransferOptions>
#include <QOpenGLShaderProgram>

#include <QQmlContext>
#include <QQuickWindow>
#include <QQuickItem>
#include <QQuickPaintedItem>
#include <QSGRenderNode>

#include <QGuiApplication>
#include <QQmlApplicationEngine>

#include <zlib.h>

#include <AL/al.h>
#include <AL/alc.h>
#include <AL/alext.h>

extern "C"
{
#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
#include <libavutil/hwcontext.h>
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
