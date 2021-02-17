#include "pch.h"
#include "LiveStreamSource.h"
#include "LiveStreamView.h"

#ifdef _WIN32
#include "D3D11SharedResource.h"
#endif

#include <QLoggingCategory>

int main(int argc, char *argv[])
{
    QCoreApplication::setAttribute(Qt::AA_EnableHighDpiScaling);

    QGuiApplication app(argc, argv);

    QLoggingCategory::setFilterRules("qt.scenegraph.general=true");

#ifdef _WIN32
    D3D11SharedResource d3d11resource;
    D3D11SharedResource::resource = &d3d11resource;
    QQuickWindow::setSceneGraphBackend(QSGRendererInterface::Direct3D11Rhi);
#endif

    QQmlApplicationEngine engine;

    qmlRegisterType<LiveStreamSource>("org.anon.QDDMonitor", 1, 0, "LiveStreamSource");
    qmlRegisterType<LiveStreamView>("org.anon.QDDMonitor", 1, 0, "LiveStreamView");

    const QUrl url(QStringLiteral("qrc:/main.qml"));
    QObject::connect(&engine, &QQmlApplicationEngine::objectCreated,
                     &app, [url](QObject *obj, const QUrl &objUrl) {
        if (!obj && url == objUrl)
            QCoreApplication::exit(-1);
    }, Qt::QueuedConnection);
    engine.load(url);

    int retcode = app.exec();

    //Join threads here

    return retcode;
}
