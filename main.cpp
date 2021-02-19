#include "pch.h"
#include "LiveStreamSource.h"
#include "LiveStreamView.h"

#ifdef _WIN32
#include "D3D11SharedResource.h"
#include "D3D11FlushHelper.h"
#endif

#include <QQmlContext>
#include <QLoggingCategory>

int main(int argc, char *argv[])
{
    qRegisterMetaType<QSharedPointer<AudioFrame>>();
    qRegisterMetaType<QSharedPointer<VideoFrame>>();

    qmlRegisterType<LiveStreamSource>("org.anon.QDDMonitor", 1, 0, "LiveStreamSource");
    qmlRegisterType<LiveStreamView>("org.anon.QDDMonitor", 1, 0, "LiveStreamView");
    qmlRegisterType<D3D11FlushHelper>("org.anon.QDDMonitor", 1, 0, "D3D11FlushHelper");

    QCoreApplication::setAttribute(Qt::AA_EnableHighDpiScaling);

    QGuiApplication app(argc, argv);

    QLoggingCategory::setFilterRules("qt.scenegraph.general=true");

#ifdef _WIN32
    D3D11SharedResource d3d11resource;
    D3D11SharedResource::resource = &d3d11resource;
    QQuickWindow::setSceneGraphBackend(QSGRendererInterface::Direct3D11Rhi);
#endif

    QQmlApplicationEngine engine;

    LiveStreamSource *source = new LiveStreamSource(nullptr);
    QThread source_thread;
    source->moveToThread(&source_thread);
    QObject::connect(&source_thread, &QThread::finished, source, &QObject::deleteLater);
    source_thread.start();
    engine.rootContext()->setContextProperty("debugSource", source);

    const QUrl url(QStringLiteral("qrc:/main.qml"));
    QObject::connect(&engine, &QQmlApplicationEngine::objectCreated,
                     &app, [url](QObject *obj, const QUrl &objUrl) {
        if (!obj && url == objUrl)
            QCoreApplication::exit(-1);
    }, Qt::QueuedConnection);
    engine.load(url);

    source->start();

    int retcode = app.exec();

    //Join threads here
    source_thread.quit();
    source_thread.wait();

    return retcode;
}
