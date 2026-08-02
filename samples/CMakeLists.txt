#
# Sample programs for libcmutils.
#
# Built with the library by default; turn them off with -DBUILD_SAMPLES=OFF.
# Each sample is a standalone executable named after its source file.
#

INCLUDE_DIRECTORIES ( ../src )
LINK_LIBRARIES ( cmutils )

# Data files (configurations, keys, documents) read by the samples. They are
# addressed through an absolute path so a sample runs from any directory.
SET ( SAMPLE_DATA_DIR "${CMAKE_CURRENT_SOURCE_DIR}/conf" )

# The plugin loaded at run time by sample_15_library.
ADD_LIBRARY ( sample_plugin SHARED sample_plugin.c )
SET_TARGET_PROPERTIES ( sample_plugin PROPERTIES
    C_VISIBILITY_PRESET default
    RUNTIME_OUTPUT_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
    LIBRARY_OUTPUT_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR} )
# It is a plain C library; it must not pull in libcmutils.
SET_TARGET_PROPERTIES ( sample_plugin PROPERTIES LINK_LIBRARIES "" )

FUNCTION ( add_sample name )
    ADD_EXECUTABLE ( ${name} ${name}.c )
    TARGET_COMPILE_DEFINITIONS ( ${name} PRIVATE
        SAMPLE_DATA_DIR="${SAMPLE_DATA_DIR}" )
    IF ( ${CMAKE_SYSTEM_NAME} STREQUAL "Windows" )
        # Put the shared library next to the executable so it can be run
        # straight from the build directory.
        ADD_CUSTOM_COMMAND ( TARGET ${name} POST_BUILD
            COMMAND ${CMAKE_COMMAND} -E copy_if_different
                    $<TARGET_FILE:cmutils> $<TARGET_FILE_DIR:${name}> )
    ENDIF ()
ENDFUNCTION ()

add_sample ( sample_01_hello )
add_sample ( sample_02_arrays )
add_sample ( sample_03_maps )
add_sample ( sample_04_strings )
add_sample ( sample_05_json )
add_sample ( sample_06_xml )
add_sample ( sample_07_config )
add_sample ( sample_08_logging )
add_sample ( sample_09_threads )
add_sample ( sample_10_threadpool )
add_sample ( sample_11_timer )
add_sample ( sample_12_pool )
add_sample ( sample_13_files )
add_sample ( sample_14_process )
add_sample ( sample_15_library )
add_sample ( sample_16_socket )
add_sample ( sample_17_dgram )
add_sample ( sample_18_http )
add_sample ( sample_19_crypto )
add_sample ( sample_20_stackwalk )

# sample_15_library resolves symbols out of the plugin built above.
ADD_DEPENDENCIES ( sample_15_library sample_plugin )
TARGET_COMPILE_DEFINITIONS ( sample_15_library PRIVATE
    SAMPLE_PLUGIN_PATH="$<TARGET_FILE:sample_plugin>" )

# Stack traces are only symbolized when the symbols survive the build.
IF ( NOT ${CMAKE_SYSTEM_NAME} STREQUAL "Windows" )
    SET_TARGET_PROPERTIES ( sample_20_stackwalk PROPERTIES
        LINK_FLAGS "-rdynamic" )
ENDIF ()
