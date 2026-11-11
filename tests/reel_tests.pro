QT += core gui quick quickcontrols2 testlib dbus
CONFIG += c++17 testcase
TARGET = reel_tests
TEMPLATE = app

INCLUDEPATH += ../src

HEADERS += ../src/framegrab.h ../src/library.h ../src/theme.h ../src/thumbnailer.h ../src/translator.h
SOURCES += reel_tests.cpp ../src/framegrab.cpp ../src/library.cpp ../src/theme.cpp ../src/thumbnailer.cpp ../src/translator.cpp
