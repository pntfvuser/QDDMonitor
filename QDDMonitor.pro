QT += quick qml network

CONFIG += c++17 qmltypes
CONFIG(release, debug|release): CONFIG += ltcg

# You can make your code fail to compile if it uses deprecated APIs.
# In order to do so, uncomment the following line.
#DEFINES += QT_DISABLE_DEPRECATED_BEFORE=0x060000    # disables all the APIs deprecated before Qt 6.0.0

HEADERS += \
    AVObjectWrapper.h \
    AudioFrame.h \
    AudioOutput.h \
    BlockingFIFOBuffer.h \
    D3D11FlushHelper.h \
    D3D11SharedResource.h \
    LiveStreamDecoder.h \
    LiveStreamSource.h \
    LiveStreamSourceBilibili.h \
    LiveStreamSourceFile.h \
    LiveStreamSourceModel.h \
    LiveStreamSubtitleOverlay.h \
    LiveStreamView.h \
    LiveStreamViewGrid.h \
    LiveStreamViewLayoutModel.h \
    LiveStreamViewModel.h \
    SubtitleFrame.h \
    VideoFrame.h \
    VideoFrameTextureNode.h \
    pch.h

PRECOMPILED_HEADER = pch.h

SOURCES += \
        AudioOutput.cpp \
        D3D11FlushHelper.cpp \
        D3D11SharedResource.cpp \
        LiveStreamDecoder.cpp \
        LiveStreamSource.cpp \
        LiveStreamSourceBilibili.cpp \
        LiveStreamSourceFile.cpp \
        LiveStreamSourceModel.cpp \
        LiveStreamSubtitleOverlay.cpp \
        LiveStreamView.cpp \
        LiveStreamViewGrid.cpp \
        LiveStreamViewLayoutModel.cpp \
        LiveStreamViewModel.cpp \
        VideoFrameTextureNode.cpp \
        main.cpp

RESOURCES += qml.qrc

QML_IMPORT_NAME = org.anon.QDDMonitor
QML_IMPORT_MAJOR_VERSION = 1

# Additional import path used to resolve QML modules in Qt Creator's code model
QML_IMPORT_PATH = $$PWD

# Additional import path used to resolve QML modules just for Qt Quick Designer
QML_DESIGNER_IMPORT_PATH =

# Default rules for deployment.
qnx: target.path = /tmp/$${TARGET}/bin
else: unix:!android: target.path = /opt/$${TARGET}/bin
!isEmpty(target.path): INSTALLS += target

DEFINES += AL_LIBTYPE_STATIC

win32: {
    INCLUDEPATH += C:/usr/include
    DEPENDPATH += C:/usr/include

    CONFIG(release, debug|release): {
        INCLUDEPATH += C:/usr/lib/Win32/Release/include
        LIBS += -LC:/usr/lib/Win32/Release
    }
    else:CONFIG(debug, debug|release): {
        INCLUDEPATH += C:/usr/lib/Win32/Debug/include
        LIBS += -LC:/usr/lib/Win32/Debug
    }

    LIBS += libavcodec.lib libavformat.lib libavutil.lib libswresample.lib libswscale.lib libx264.lib x265.lib OpenAL32.lib libssl.lib libcrypto.lib evr.lib mf.lib strmiids.lib mfplat.lib mfplay.lib mfreadwrite.lib mfuuid.lib ws2_32.lib bcrypt.lib secur32.lib d3d11.lib
}
else:unix: {
    LIBS += -lavcodec -lavformat -lavutil
}
