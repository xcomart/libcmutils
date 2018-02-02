
CMAKE_MINIMUM_REQUIRED ( VERSION 2.4 )

PROJECT ( cmutils )

SET ( VERSION_MAJOR "1" )
SET ( VERSION_MINOR "0" )
SET ( VERSION_PATCH "0" )
SET ( LIB_VERSION_STRING ${VERSION_MAJOR}.${VERSION_MINOR}.${VERSION_PATCH} )

OPTION ( SUPPORT_SSL "Support SSL in networking" ON )

SET ( LIB_SRCS
    src/arrays.c
    src/base.c
    src/callstack.c
    src/concurrent.c
    src/config.c
    src/lists.c
    src/logger.c
    src/maps.c
    src/memdebug.c
    src/nanojson.c
    src/nanoxml.c
    src/network.c
    src/nio.c
    src/pattern.c
    src/pool.c
    src/strings.c
    src/system.c )

ADD_LIBRARY ( cmutils SHARED ${LIB_SRCS} )
ADD_LIBRARY ( cmutilsStatic STATIC ${LIB_SRCS} )

FIND_PACKAGE ( PkgConfig REQUIRED )

IF ( SUPPORT_SSL )
    PKG_CHECK_MODULES ( OPENSSL REQUIRED openssl )
    TARGET_LINK_LIBRARIES ( cmutils ${OPENSSL_LIBRARIES} )
    TARGET_INCLUDE_DIRECTORIES( cmutils PUBLIC ${OPENSSL_INCLUDE_DIRS} )
    TARGET_COMPILE_OPTIONS( cmutils PUBLIC ${OPENSSL_CFLAGS_OTHER} )
    TARGET_LINK_LIBRARIES ( cmutilsStatic ${OPENSSL_LIBRARIES} )
    TARGET_INCLUDE_DIRECTORIES( cmutilsStatic PUBLIC ${OPENSSL_INCLUDE_DIRS} )
    TARGET_COMPILE_OPTIONS( cmutilsStatic PUBLIC ${OPENSSL_CFLAGS_OTHER} )
    ADD_DEFINITIONS ( -DCMUTIL_SUPPORT_SSL -DCMUTIL_SSL_USE_OPENSSL)
ENDIF ( SUPPORT_SSL )

SET_TARGET_PROPERTIES ( cmutils PROPERTIES VERSION ${LIB_VERSION_STRING} )
SET_TARGET_PROPERTIES ( cmutilsStatic PROPERTIES OUTPUT_NAME cmutils )

INSTALL(TARGETS cmutils cmutilsStatic
  LIBRARY DESTINATION lib
  ARCHIVE DESTINATION lib )
INSTALL(FILES src/libcmutils.h DESTINATION include)
