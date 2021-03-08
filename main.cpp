#include "pch.h"

#include "LiveStreamSource.h"
#include "LiveStreamSourceModel.h"
#include "LiveStreamView.h"
#include "LiveStreamViewModel.h"
#include "LiveStreamViewLayoutModel.h"
#include "LiveStreamViewGrid.h"
#include "AudioOutput.h"

#ifdef _WIN32
#include "D3D11SharedResource.h"
#include "D3D11FlushHelper.h"
#endif

#include <QQmlContext>
#include <QLoggingCategory>

int main(int argc, char *argv[])
{
    qRegisterMetaType<const AVCodecContext *>();
    qRegisterMetaType<QSharedPointer<AudioFrame>>();
    qRegisterMetaType<QSharedPointer<VideoFrame>>();
    qRegisterMetaType<QSharedPointer<SubtitleFrame>>();

    qmlRegisterAnonymousType<LiveStreamSource>("org.anon.QDDMonitor", 1);
    qmlRegisterAnonymousType<AudioOutput>("org.anon.QDDMonitor", 1);
    qmlRegisterAnonymousType<LiveStreamViewGridAttachedType>("org.anon.QDDMonitor", 1);

    qmlRegisterType<LiveStreamSourceModel>("org.anon.QDDMonitor", 1, 0, "LiveStreamSourceModel");
    qmlRegisterType<LiveStreamViewModel>("org.anon.QDDMonitor", 1, 0, "LiveStreamViewModel");
    qmlRegisterType<LiveStreamViewLayoutModel>("org.anon.QDDMonitor", 1, 0, "LiveStreamViewLayoutModel");
    qmlRegisterType<LiveStreamViewGrid>("org.anon.QDDMonitor", 1, 0, "LiveStreamViewGrid");
    qmlRegisterType<LiveStreamView>("org.anon.QDDMonitor", 1, 0, "LiveStreamView");
    qmlRegisterType<D3D11FlushHelper>("org.anon.QDDMonitor", 1, 0, "D3D11FlushHelper");

    QCoreApplication::setAttribute(Qt::AA_EnableHighDpiScaling);

    QGuiApplication app(argc, argv);

    QLoggingCategory::setFilterRules("qddm.video=false\n"
                                     "qddm.audio=false\n"
                                     "qddm.decode=false\n"
                                     //"qddm.sourcectrl=false\n"
                                     "qt.scenegraph.general=true");

#ifdef _WIN32
    D3D11SharedResource d3d11resource;
    D3D11SharedResource::resource = &d3d11resource;
    QQuickWindow::setSceneGraphBackend(QSGRendererInterface::Direct3D11Rhi);
#endif

    QQmlApplicationEngine engine;

    const QUrl url(QStringLiteral("qrc:/main.qml"));
    QObject::connect(&engine, &QQmlApplicationEngine::objectCreated,
                     &app, [url](QObject *obj, const QUrl &objUrl) {
        if (!obj && url == objUrl)
            QCoreApplication::exit(-1);
    }, Qt::QueuedConnection);
    engine.load(url);

    int retcode = app.exec();

    return retcode;
}
