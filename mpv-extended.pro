QT += core gui gui-private network qml quick quickcontrols2 dbus opengl

CONFIG += c++17 release link_pkgconfig
PKGCONFIG += mpv wayland-client
TARGET = mpv-extended
VERSION = 0.1.0  # keep in step with pkgbuild/PKGBUILD
DEFINES += APP_VERSION=\\\"$$VERSION\\\"
# bin/build CONFIG+=dev adds the MPV_EXTENDED_GRAB screenshot hook (see main.cpp).
dev: DEFINES += MPV_EXTENDED_DEV
TEMPLATE = app

HEADERS += \
    src/filepicker.h \
    src/framegrab.h \
    src/idleinhibit.h \
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
    src/idleinhibit.cpp \
    src/inhibitor.cpp \
    src/library.cpp \
    src/mpvobject.cpp \
    src/theme.cpp \
    src/thumbnailer.cpp \
    src/translator.cpp

RESOURCES += src/resources.qrc

# Wayland idle-inhibit, generated from the protocol description at build time.
WAYLAND_PROTOCOLS_DIR = $$system(pkg-config --variable=pkgdatadir wayland-protocols)
WAYLAND_XML = $$WAYLAND_PROTOCOLS_DIR/unstable/idle-inhibit/idle-inhibit-unstable-v1.xml
wayland_header.input = WAYLAND_XML
wayland_header.output = ${QMAKE_FILE_BASE}-client-protocol.h
wayland_header.commands = wayland-scanner client-header ${QMAKE_FILE_IN} ${QMAKE_FILE_OUT}
wayland_header.variable_out = HEADERS
wayland_header.CONFIG += target_predeps no_link
wayland_code.input = WAYLAND_XML
wayland_code.output = ${QMAKE_FILE_BASE}-protocol.c
wayland_code.commands = wayland-scanner private-code ${QMAKE_FILE_IN} ${QMAKE_FILE_OUT}
wayland_code.variable_out = SOURCES
wayland_code.depends = ${QMAKE_FILE_BASE}-client-protocol.h
QMAKE_EXTRA_COMPILERS += wayland_header wayland_code
