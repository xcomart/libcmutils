#
# API reference for libcmutils.
#
# Adds a "docs" target when doxygen is available. It is never part of "all":
# the reference is built on request, not on every compile.
#

FIND_PACKAGE ( Doxygen )

IF ( NOT DOXYGEN_FOUND )
    MESSAGE ( STATUS "Doxygen not found, the docs target is unavailable" )
    RETURN ()
ENDIF ()

# The Doxyfile reads the version out of the environment so that a bare
# "doxygen" run in this directory works too.
ADD_CUSTOM_TARGET ( docs
    COMMAND ${CMAKE_COMMAND} -E env
            LIBCMUTILS_VERSION=${LIB_VERSION_STRING}
            ${DOXYGEN_EXECUTABLE} Doxyfile
    WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
    COMMENT "Generating the API reference into doc/html"
    VERBATIM )

MESSAGE ( STATUS "Doxygen found, \"docs\" target available" )
