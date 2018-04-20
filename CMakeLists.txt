
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
    src/datagram.c
    src/http.c
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

IF ( NOT ${CMAKE_SYSTEM_NAME} STREQUAL "Linux" )
    FIND_PACKAGE ( Iconv REQUIRED )
    TARGET_LINK_LIBRARIES ( cmutils ${Iconv_LIBRARIES} )
    TARGET_LINK_LIBRARIES ( cmutilsStatic ${Iconv_LIBRARIES} )
ENDIF ()

IF ( SUPPORT_SSL )

    SET (OPENSSL_PREFIX "" CACHE PATH "The path to the prefix of a OpenSSL installation")

    FIND_LIBRARY ( OPENSSL_LIBRARY NAMES ssl
        PATHS ${OPENSSL_PREFIX}/lib /usr/lib /usr/local/lib /usr/local/opt/openssl/lib )
    MESSAGE ( "OpenSSL library found: ${OPENSSL_LIBRARY}")

    FIND_PATH ( OPENSSL_INCLUDE_DIRS NAMES ssl.h
        PATHS ${OPENSSL_PREFIX}/include /usr/include /usr/local/include /usr/local/opt/openssl/include
        PATH_SUFFIXES openssl )
    MESSAGE ( "OpenSSL header found: ${OPENSSL_INCLUDE_DIRS}")
    
    IF ( OPENSSL_INCLUDE_DIRS AND OPENSSL_LIBRARY )
        GET_FILENAME_COMPONENT ( OPENSSL_LIBRARY_DIR ${OPENSSL_LIBRARY} PATH )
        # remove suffix 'openssl'
        GET_FILENAME_COMPONENT ( OPENSSL_INCLUDE_DIRS ${OPENSSL_INCLUDE_DIRS} PATH )
        SET ( OPENSSL_LIBRARIES ssl crypto )
        SET ( OPENSSL_FOUND TRUE )
    ENDIF ()

    IF ( OPENSSL_FOUND )
        MESSAGE ( STATUS "Found OpenSSL: ${OPENSSL_LIBRARY}" )
    ELSEIF ( OPENSSL_FOUND )
        MESSAGE ( FATAL_ERROR "Could not find OpenSSL" )
    ENDIF ()

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
