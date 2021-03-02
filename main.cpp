#include "pch.h"
#include "LiveStreamSource.h"
#include "LiveStreamSourceModel.h"
#include "LiveStreamView.h"
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

    qmlRegisterType<LiveStreamSourceModel>("org.anon.QDDMonitor", 1, 0, "LiveStreamSourceModel");
    qmlRegisterType<LiveStreamView>("org.anon.QDDMonitor", 1, 0, "LiveStreamView");
    qmlRegisterType<D3D11FlushHelper>("org.anon.QDDMonitor", 1, 0, "D3D11FlushHelper");

    QCoreApplication::setAttribute(Qt::AA_EnableHighDpiScaling);

    QGuiApplication app(argc, argv);

    QLoggingCategory::setFilterRules("qddm.video=false\n"
                                     //"qddm.decode=false\n"
                                     "qt.scenegraph.general=true");

#ifdef _WIN32
    D3D11SharedResource d3d11resource;
    D3D11SharedResource::resource = &d3d11resource;
    QQuickWindow::setSceneGraphBackend(QSGRendererInterface::Direct3D11Rhi);
#endif

    QQmlApplicationEngine engine;

    AudioOutput *audio_out = new AudioOutput;
    QThread audio_thread;
    audio_out->moveToThread(&audio_thread);
    QObject::connect(&audio_thread, &QThread::finished, audio_out, &QObject::deleteLater);
    audio_thread.start();
    engine.rootContext()->setContextProperty("debugAudioOut", audio_out);

    //LiveStreamSourceFile *source = new LiveStreamSourceFile(nullptr);
    QNetworkAccessManager *network_manager = new QNetworkAccessManager();
    LiveStreamSourceBilibili *source = new LiveStreamSourceBilibili(21919321, network_manager);
    QThread source_thread;
    network_manager->moveToThread(&source_thread);
    source->moveToThread(&source_thread);
    QObject::connect(&source_thread, &QThread::finished, network_manager, &QObject::deleteLater);
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

    //source->setFilePath("E:\\Home\\Documents\\Qt\\QDDMonitor\\testsrc.mkv");
    source->connect(source, &LiveStreamSourceBilibili::infoUpdated, source, [source](int status, const QString &, const QList<QString> &options) { if (status == LiveStreamSource::STATUS_ONLINE) source->onRequestActivate(options.empty() ? "" : options.front()); });
    QMetaObject::invokeMethod(source, "onRequestUpdateInfo");

    int retcode = app.exec();

    //Join threads here
    source_thread.quit();
    source_thread.wait();
    audio_thread.quit();
    audio_thread.wait();

    return retcode;
}
