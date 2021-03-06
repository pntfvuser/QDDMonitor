QT += quick qml network svg websockets

CONFIG += c++17
CONFIG(release, debug|release): CONFIG += ltcg

# You can make your code fail to compile if it uses deprecated APIs.
# In order to do so, uncomment the following line.
#DEFINES += QT_DISABLE_DEPRECATED_BEFORE=0x060000    # disables all the APIs deprecated before Qt 6.0.0

HEADERS += \
    AVObjectWrapper.h \
    AudioFrame.h \
    AudioOutput.h \
    BlockingFIFOBuffer.h \
    FixedGridLayout.h \
    LiveStreamDecoder.h \
    LiveStreamSource.h \
    LiveStreamSourceBilibili.h \
    LiveStreamSourceBilibiliDanmu.h \
    LiveStreamSourceFile.h \
    LiveStreamSourceModel.h \
    LiveStreamSubtitleOverlay.h \
    LiveStreamView.h \
    LiveStreamViewLayoutModel.h \
    LiveStreamViewModel.h \
    SubtitleFrame.h \
    VideoFrame.h \
    VideoFrameRenderNodeOGL.h \
    pch.h

PRECOMPILED_HEADER = pch.h
QMAKE_MOC_OPTIONS += -b pch.h

SOURCES += \
        AudioOutput.cpp \
        FixedGridLayout.cpp \
        LiveStreamDecoder.cpp \
        LiveStreamSource.cpp \
        LiveStreamSourceBilibili.cpp \
        LiveStreamSourceBilibiliDanmu.cpp \
        LiveStreamSourceFile.cpp \
        LiveStreamSourceModel.cpp \
        LiveStreamSubtitleOverlay.cpp \
        LiveStreamView.cpp \
        LiveStreamViewLayoutModel.cpp \
        LiveStreamViewModel.cpp \
        VideoFrameRenderNodeOGL.cpp \
        main.cpp

RESOURCES += qml.qrc \
    lang.qrc \
    res.qrc \
    shader.qrc

TRANSLATIONS += localization/qddmonitor_zh.ts

# Additional import path used to resolve QML modules in Qt Creator's code model
QML_IMPORT_PATH =

# Additional import path used to resolve QML modules just for Qt Quick Designer
QML_DESIGNER_IMPORT_PATH =

# Default rules for deployment.
qnx: target.path = /tmp/$${TARGET}/bin
else: unix:!android: target.path = /opt/$${TARGET}/bin
!isEmpty(target.path): INSTALLS += target

win32: {
    DEFINES += AL_LIBTYPE_STATIC ZLIB_WINAPI

    INCLUDEPATH += C:/usr/include
    DEPENDPATH += C:/usr/include

    contains(QT_ARCH, x86_64): {
        CONFIG(release, debug|release): {
            INCLUDEPATH += C:/usr/lib/x64/Release/include
            LIBS += -LC:/usr/lib/x64/Release
        }
        else:CONFIG(debug, debug|release): {
            INCLUDEPATH += C:/usr/lib/x64/Debug/include
            LIBS += -LC:/usr/lib/x64/Debug
        }
    } else: {
        CONFIG(release, debug|release): {
            INCLUDEPATH += C:/usr/lib/Win32/Release/include
            LIBS += -LC:/usr/lib/Win32/Release
        }
        else:CONFIG(debug, debug|release): {
            INCLUDEPATH += C:/usr/lib/Win32/Debug/include
            LIBS += -LC:/usr/lib/Win32/Debug
        }
    }

    LIBS += libavcodec.lib libavformat.lib libavutil.lib libswresample.lib libswscale.lib libx264.lib x265.lib OpenAL32.lib libssl.lib libcrypto.lib zlibstat.lib evr.lib mf.lib strmiids.lib mfplat.lib mfplay.lib mfreadwrite.lib mfuuid.lib ws2_32.lib bcrypt.lib secur32.lib
}
else:unix: {
    LIBS += -lavcodec -lavformat -lavutil -lswresample -lswscale -lopenal -lz
}
