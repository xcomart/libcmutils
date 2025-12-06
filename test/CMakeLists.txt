

include_directories(../src)
link_libraries(cmutilsStatic)

add_executable(array_test array_test.c)

add_test(NAME array_test COMMAND array_test)