QT += core gui network quick quickcontrols2 testlib dbus
CONFIG += c++17 testcase
TARGET = tests
TEMPLATE = app

INCLUDEPATH += ../src

HEADERS += ../src/framegrab.h ../src/library.h ../src/theme.h ../src/thumbnailer.h ../src/translator.h
SOURCES += tests.cpp ../src/framegrab.cpp ../src/library.cpp ../src/theme.cpp ../src/thumbnailer.cpp ../src/translator.cpp
