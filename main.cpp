#include "pch.h"

#include "LiveStreamSource.h"
#include "LiveStreamSourceModel.h"
#include "LiveStreamView.h"
#include "LiveStreamViewModel.h"
#include "LiveStreamViewLayoutModel.h"
#include "LiveStreamSubtitleOverlay.h"
#include "FixedGridLayout.h"
#include "AudioOutput.h"

#include <QLoggingCategory>
#include <QTranslator>

int main(int argc, char *argv[])
{
    qRegisterMetaType<const AVCodecContext *>();
    qRegisterMetaType<QSharedPointer<AudioFrame>>();
    qRegisterMetaType<QSharedPointer<VideoFrame>>();
    qRegisterMetaType<QSharedPointer<SubtitleFrame>>();

#if (QT_VERSION >= QT_VERSION_CHECK(5, 14, 0))
    qmlRegisterAnonymousType<AudioOutput>("org.anon.QDDMonitor", 1);
    qmlRegisterAnonymousType<FixedGridLayoutAttachedType>("org.anon.QDDMonitor", 1);
    qmlRegisterAnonymousType<LiveStreamSource>("org.anon.QDDMonitor", 1);
    qmlRegisterAnonymousType<LiveStreamSourceInfo>("org.anon.QDDMonitor", 1);
    qmlRegisterAnonymousType<LiveStreamSubtitleOverlay>("org.anon.QDDMonitor", 1);
#else
    qmlRegisterUncreatableType<AudioOutput>("org.anon.QDDMonitor", 1, 0, "AudioOutput", "This type is intended to only be created in C++");
    qmlRegisterUncreatableType<FixedGridLayoutAttachedType>("org.anon.QDDMonitor", 1, 0, "FixedGridLayoutAttachedType", "This type is intended to only be created in C++");
    qmlRegisterUncreatableType<LiveStreamSource>("org.anon.QDDMonitor", 1, 0, "LiveStreamSource", "This type is intended to only be created in C++");
    qmlRegisterUncreatableType<LiveStreamSourceInfo>("org.anon.QDDMonitor", 1, 0, "LiveStreamSourceInfo", "This type is intended to only be created in C++");
    qmlRegisterUncreatableType<LiveStreamSubtitleOverlay>("org.anon.QDDMonitor", 1, 0, "LiveStreamSubtitleOverlay", "This type is intended to only be created in C++");
#endif

    qmlRegisterType<LiveStreamSourceModel>("org.anon.QDDMonitor", 1, 0, "LiveStreamSourceModel");
    qmlRegisterType<LiveStreamViewModel>("org.anon.QDDMonitor", 1, 0, "LiveStreamViewModel");
    qmlRegisterType<LiveStreamViewLayoutModel>("org.anon.QDDMonitor", 1, 0, "LiveStreamViewLayoutModel");
    qmlRegisterType<FixedGridLayout>("org.anon.QDDMonitor", 1, 0, "FixedGridLayout");
    qmlRegisterType<LiveStreamView>("org.anon.QDDMonitor", 1, 0, "LiveStreamView");

    QCoreApplication::setAttribute(Qt::AA_EnableHighDpiScaling);

    QGuiApplication app(argc, argv);

    QTranslator translator;
    translator.load(QLocale(), QLatin1String("qddmonitor"), QLatin1String("_"), QLatin1String(":/localization"), QLatin1String(".qm"));
    app.installTranslator(&translator);

    QLoggingCategory::setFilterRules("qddm.video=false\n"
                                     //"qddm.audio=false\n"
                                     "qddm.decode=false\n"
                                     "qddm.sourcectrl=false\n"
                                     //"qt.scenegraph.general=true"
                                     );

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
