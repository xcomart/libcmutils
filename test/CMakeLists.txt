

include_directories(../src)
link_libraries(cmutils)

function(add_test_target)
    add_executable(${ARGV} ${ARGV}.c)
    add_test(NAME ${ARGV} COMMAND ${ARGV})
endfunction()

add_test_target(array_test)
add_test_target(concurrent_test)
add_test_target(config_test)
add_test_target(dgram_test)
