QT += quick

CONFIG += c++17 metatypes
CONFIG(release, debug|release): CONFIG += ltcg

# You can make your code fail to compile if it uses deprecated APIs.
# In order to do so, uncomment the following line.
#DEFINES += QT_DISABLE_DEPRECATED_BEFORE=0x060000    # disables all the APIs deprecated before Qt 6.0.0

HEADERS += \
    AVObjectWrapper.h \
    AudioFrame.h \
    D3D11FlushHelper.h \
    D3D11SharedResource.h \
    LiveStreamSource.h \
    LiveStreamSourceModel.h \
    LiveStreamView.h \
    VideoFrame.h \
    VideoFrameTextureNode.h \
    pch.h

PRECOMPILED_HEADER = pch.h

SOURCES += \
        D3D11FlushHelper.cpp \
        D3D11SharedResource.cpp \
        LiveStreamSource.cpp \
        LiveStreamSourceModel.cpp \
        LiveStreamView.cpp \
        VideoFrameTextureNode.cpp \
        main.cpp

RESOURCES += qml.qrc

# Additional import path used to resolve QML modules in Qt Creator's code model
QML_IMPORT_PATH =

# Additional import path used to resolve QML modules just for Qt Quick Designer
QML_DESIGNER_IMPORT_PATH =

# Default rules for deployment.
qnx: target.path = /tmp/$${TARGET}/bin
else: unix:!android: target.path = /opt/$${TARGET}/bin
!isEmpty(target.path): INSTALLS += target

win32: {
    CONFIG(release, debug|release): LIBS += -LC:/usr/lib/Release
    else:CONFIG(debug, debug|release): LIBS += -LC:/usr/lib/Debug
    LIBS += libavcodec.lib libavformat.lib libavutil.lib libswresample.lib libswscale.lib libx264.lib x265.lib evr.lib mf.lib strmiids.lib mfplat.lib mfplay.lib mfreadwrite.lib mfuuid.lib ws2_32.lib bcrypt.lib secur32.lib d3d11.lib

    INCLUDEPATH += C:/usr/include
    DEPENDPATH += C:/usr/include
}
else:unix: {
    LIBS += -lavcodec -lavformat -lavutil
}
