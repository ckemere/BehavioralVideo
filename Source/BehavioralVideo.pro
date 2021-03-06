#-------------------------------------------------
#
# Project created by QtCreator 2014-01-13T15:17:20
#
#-------------------------------------------------

QT += core gui
QT += multimedia multimediawidgets
QT += serialport
QT += uitools
greaterThan(QT_MAJOR_VERSION, 4): QT += widgets

TARGET = BehavioralVideo
TEMPLATE = app

unix: INCLUDEPATH += /usr/include /usr/include/flycapture
unix: LIBS += -L"/usr/lib" -lavcodec -lavformat -lswscale -lavutil -lflycapture -lv4l2 -lv4lconvert
#-lrt -ldl -ljpeg
unix: QMAKE_CXXFLAGS += -D__STDC_CONSTANT_MACROS

QMAKE_CFLAGS += -g -O3

SOURCES += main.cpp\
        mainwindow.cpp \
    VideoGLWidget.cpp \
    VideoWriter.cpp \
    CameraInterfaces/FakeCamera.cpp \
    CameraInterfaces/PtGrey.cpp \
    dummycameracontroller.cpp \
    SerialCameraController.cpp \
    CameraInterfaces/GenericCamera.cpp \
    GenericCameraController.cpp \
    DataController.cpp \
    CameraInterfaces/SupportedCamera.cpp \
    CameraInterfaces/V4L2/ctrl-tab.cpp \
    CameraInterfaces/V4L2/general-tab.cpp \
    CameraInterfaces/V4L2/qv4l2.cpp \
    CameraInterfaces/V4L2/v4l2-api.cpp

HEADERS  += mainwindow.h \
    FFMPEG.h \
    VideoGLWidget.h \
    VideoWriter.h \
    CameraInterfaces/FakeCamera.h \
    CameraInterfaces/PtGrey.h \
    Threads.h \
    dummycameracontroller.h \
    SerialCameraController.h \
    GenericCamera.h \
    GenericCameraController.h \
    datacontroller.h \
    DataController.h \
    CameraInterfaces/SupportedCamera.h \
    CameraInterfaces/V4L2/general-tab.h \
    CameraInterfaces/V4L2/v4l2-api.h

FORMS    += mainwindow.ui \
    preferences.ui

RESOURCES += \
    icons.qrc

