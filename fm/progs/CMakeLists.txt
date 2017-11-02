cmake_minimum_required(VERSION 2.8)

FIND_PACKAGE (Threads)

set (TESTS)
if (NOT NO_SOCKETS)
    set (PROGS format_dump format_cmd format_info)
    ADD_EXECUTABLE(format_server format_server.c server.c)
    TARGET_LINK_LIBRARIES(format_server ffs ${CMAKE_THREAD_LIBS_INIT})
else ()
    set (PROGS)
endif()
INCLUDE_DIRECTORIES(BEFORE ${ffs_BINARY_DIR} ${ffs_SOURCE_DIR}${ffs_SOURCE_DIR}/fm )

foreach (TEST ${TESTS} )
    ADD_EXECUTABLE(${TEST} ${TEST}.c)
    TARGET_LINK_LIBRARIES(${TEST} ffs )
    ADD_TEST(${TEST} "${TEST}")
endforeach()

foreach (PROG ${PROGS} )
    ADD_EXECUTABLE(${PROG} ${PROG}.c)
    TARGET_LINK_LIBRARIES(${PROG} ffs )
endforeach()

SET(PROGS_INSTALL)
# prefix and suffix elements
foreach(l ${PROGS} format_server)
   list(APPEND PROGS_INSTALL ${CMAKE_CURRENT_BINARY_DIR}/${l} )
endforeach()

INSTALL(PROGRAMS ${PROGS_INSTALL} DESTINATION bin)

