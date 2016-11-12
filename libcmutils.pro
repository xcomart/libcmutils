TEMPLATE = lib
TARGET = cmutils
CONFIG -= qt

DEFINES += CMUTILS_LIBRARY

BUILD_DIR = ../build
COPY_CMD = cp
LIB_DIR = lib
win32 {
    LIB_DIR = bin
}

CONFIG(debug, debug|release) {
	DEFINES += DEBUG
    DESTDIR = $$BUILD_DIR/$$LIB_DIR/debug
}

CONFIG(release, debug|release) {
    DEFINES += DEBUG
    DESTDIR = $$BUILD_DIR/$$LIB_DIR/release
}

unix {
	target.path = /usr/lib
	INSTALLS += target
}
win32 {
	LIBS += -lws2_32
#    COPY_CMD = copy
}

!linux {
	LIBS += -liconv
}

QMAKE_CFLAGS += -Wno-pointer-to-int-cast -Wno-int-to-pointer-cast

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
    VERSION

INCLUDE_TARGET.commands = $$COPY_CMD ../libcmutils/src/libcmutils.h $$BUILD_DIR/include

QMAKE_EXTRA_TARGETS += INCLUDE_TARGET

POST_TARGETDEPS += INCLUDE_TARGET

