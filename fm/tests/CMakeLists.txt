cmake_minimum_required(VERSION 2.8)

set (TESTS align_test compat_test)
set (PROGS format_test self_format_test)

ENABLE_TESTING()

INCLUDE_DIRECTORIES(BEFORE ${ffs_BINARY_DIR} ${ffs_SOURCE_DIR}${ffs_SOURCE_DIR}/fm ${ffs_BINARY_DIR}/fm)

foreach (TEST ${TESTS} )
    ADD_EXECUTABLE(${TEST} ${TEST}.c test_funcs.c)
    TARGET_LINK_LIBRARIES(${TEST} fm )
    ADD_TEST(${TEST} "${TEST}")
endforeach()

foreach (PROG ${PROGS} )
    ADD_EXECUTABLE(${PROG} ${PROG}.c test_funcs.c)
    TARGET_LINK_LIBRARIES(${PROG} fm )
endforeach()

INSTALL(PROGRAMS ${CMAKE_CURRENT_BINARY_DIR}/format_test DESTINATION bin)
