QT += core gui qml quick quickcontrols2 dbus opengl

CONFIG += c++17 release link_pkgconfig
PKGCONFIG += mpv
TARGET = reel
TEMPLATE = app

HEADERS += \
    src/filepicker.h \
    src/framegrab.h \
    src/inhibitor.h \
    src/library.h \
    src/mpvobject.h \
    src/theme.h \
    src/thumbnailer.h \
    src/translator.h

SOURCES += \
    src/main.cpp \
    src/filepicker.cpp \
    src/framegrab.cpp \
    src/inhibitor.cpp \
    src/library.cpp \
    src/mpvobject.cpp \
    src/theme.cpp \
    src/thumbnailer.cpp \
    src/translator.cpp

RESOURCES += src/resources.qrc
