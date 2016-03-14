CONFIG -= qt
TARGET = cmutils
TEMPLATE = lib

DEFINES += CMUTILS_LIBRARY

CONFIG(debug, debug|release) {
	DEFINES += DEBUG
}

unix {
	target.path = /usr/lib
	INSTALLS += target
}
win32 {
	LIBS += -lws2_32
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
    src/pattern.h \
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
    demo/Makefile.in
