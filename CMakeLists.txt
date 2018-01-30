
CMAKE_MINIMUM_REQUIRED ( VERSION 2.4 )

PROJECT ( cmutils )

OPTION ( SUPPORT_SSL "Support SSL in networking" ON )

IF ( SUPPORT_SSL )
    FIND_PACKAGE ( OpenSSL REQUIRED )
    INCLUDE_DIRECTORIES ( ${OpenSSL_INCLUDE_DIRS} )
    SET ( LIBS ${LIBS} ${OpenSSL_LIBRARIES} )
    ADD_DEFINITIONS ( -DCMUTIL_SUPPORT_SSL -DCMUTIL_SSL_USE_OPENSSL)
ENDIF ( SUPPORT_SSL )

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

SET_TARGET_PROPERTIES ( cmutilsStatic PROPERTIES OUTPUT_NAME cmutils)

ADD_CUSTOM_COMMAND ( TARGET cmutils
    POST_BUILD
    COMMAND "${CMAKE_COMMAND}" -E copy 
        "$<TARGET_FILE:cmutils>"
        "../../build/lib/$<TARGET_FILE_NAME:cmutils>" 
    COMMENT "Copying shared library to output directory")

ADD_CUSTOM_COMMAND ( TARGET cmutilsStatic
    POST_BUILD
    COMMAND "${CMAKE_COMMAND}" -E copy 
        "$<TARGET_FILE:cmutilsStatic>"
        "../../build/lib/$<TARGET_FILE_NAME:cmutilsStatic>" 
    COMMENT "Copying static library to output directory")

ADD_CUSTOM_COMMAND ( TARGET cmutils
    POST_BUILD
    COMMAND "${CMAKE_COMMAND}" -E copy 
        "../src/libcmutils.h"
        "../../build/include/." 
    COMMENT "Copying header file to output directory")
    