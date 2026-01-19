QMAKE_PROJECT_DEPTH = 0

QT += core gui widgets webenginewidgets

CONFIG += c++17
QMAKE_ENV += chcp 936
FORMS += mainwindow.ui

TARGET = hjdz
TEMPLATE = app
LIBS += -luser32 -lgdi32
SOURCES += \
    automationpanel.cpp \
    automationworker.cpp \
    main.cpp \
    mainwindow.cpp \
    taskmodel.cpp \
    scriptrunner.cpp \
    screencapture.cpp \
    taskeditor.cpp \
    stepwidget.cpp

HEADERS += \
    SilentWebPage.h \
    StopToken.h \
    automationpanel.h \
    automationworker.h \
    fsm_framework.h \
    imgdsl_qt.h \
    mainwindow.h \
    mywebpage.h \
    taskmodel.h \
    scriptrunner.h \
    screencapture.h \
    taskeditor.h \
    stepwidget.h

# Use UTF-8 for MSVC so Chinese strings are safe
QMAKE_CXXFLAGS += /utf-8

# -------- OpenCV headers & libs --------
INCLUDEPATH += D:/hjdz/opencv/build/include
# Link the right lib per config
CONFIG(debug, debug|release) {
    LIBS += -LD:/hjdz/hjdz -lopencv_world4120d
} else {
    LIBS += -LD:/hjdz/hjdz -lopencv_world4120
}


# ---- Detect build out dir (qmake-time) ----
win32 {
    CONFIG(debug, debug|release) {
        OPENCV_DLL = "$$PWD/opencv_world4120d.dll"
    } else {
        OPENCV_DLL = "$$PWD/opencv_world4120.dll"
    }

    FLASH_DLL = "$$PWD/pepflashplayer.dll"
    DESTDIR_WIN = "$$OUT_PWD"

    message(DESTDIR_WIN = $$DESTDIR_WIN)
    message(OPENCV_DLL = $$OPENCV_DLL)
    message(FLASH_DLL = $$FLASH_DLL)

    # 去掉结尾的反斜杠，并加引号
    QMAKE_POST_LINK = $$quote(cmd /c copy /Y \"$$PWD\\opencv_world4120.dll\" \"$$OUT_PWD\\opencv_world4120.dll\" && copy /Y \"$$PWD\\pepflashplayer.dll\" \"$$OUT_PWD\\pepflashplayer.dll\")
}



