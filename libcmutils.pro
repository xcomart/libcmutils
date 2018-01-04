TEMPLATE = lib
TARGET = cmutils
CONFIG -= qt

VER_MAJ = 0
VER_MIN = 1
VER_PAT = 0

DEFINES += CMUTILS_LIBRARY

CONFIG += SUPPORT_SSL

BUILD_DIR = ../build
COPY_CMD = cp
LIB_DIR = lib
win32 {
    LIB_DIR = bin
}

DESTDIR = $$BUILD_DIR/$$LIB_DIR

CONFIG(debug, debug|release) {
    DEFINES += DEBUG
}

!win32-msvc* {
    CONFIG += link_pkgconfig
}

unix {
    target.path = /usr/lib
    INSTALLS += target
}
win32 {
    LIBS += -lws2_32
}

win32-msvc* {
    COPY_CMD = copy
    DEFINES += _CRT_SECURE_NO_WARNINGS _WINSOCK_DEPRECATED_NO_WARNINGS
}

!linux {
    !win32-msvc* {
        LIBS += -liconv
    }
}

contains (CONFIG, SUPPORT_SSL) {
    DEFINES += CMUTIL_SUPPORT_SSL
    DEFINES += CMUTIL_SSL_USE_OPENSSL
    !win32-msvc* {
#        PKGCONFIG += gnutls
        PKGCONFIG += openssl
    }
}

SOURCES += \
    src/arrays.c \
    src/base.c \
    src/callstack.c \
    src/concurrent.c \
    src/config.c \
    src/lists.c \
    src/logger.c \
    src/maps.c \
    src/memdebug.c \
    src/nanojson.c \
    src/nanoxml.c \
    src/network.c \
    src/pattern.c \
    src/pool.c \
    src/strings.c \
    src/system.c

HEADERS += \
    src/functions.h \
    src/libcmutils.h \
    src/platforms.h

DISTFILES += \
    LICENSE \
    README.md \
    compile \
    configure \
    depcomp \
    install-sh \
    missing \
    config.h.in \
    configure.ac \
    NEWS \
    reconf \
    AUTHORS \
    ChangeLog \
    COPYING \
    INSTALL \
    Makefile.am \
    Makefile.in \
    demo/Makefile.am \
    demo/Makefile.in \
    VERSION \
    demo/cmutil_log.json

!win32-msvc* {
    INCLUDE_TARGET.commands += mkdir -p $$BUILD_DIR/include;\
            $$COPY_CMD ../libcmutils/src/libcmutils.h $$BUILD_DIR/include/.

    QMAKE_EXTRA_TARGETS += INCLUDE_TARGET

    POST_TARGETDEPS += INCLUDE_TARGET
}

