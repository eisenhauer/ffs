cmake_minimum_required(VERSION 2.8)

set (TESTS)
if (NOT NO_SOCKETS)
    set (PROGS FFSdump FFScp FFSsort)
else ()
    set (PROGS)
endif()
INCLUDE_DIRECTORIES(BEFORE ${ffs_BINARY_DIR} ${ffs_SOURCE_DIR}${ffs_SOURCE_DIR}/fm )

foreach (TEST ${TESTS} )
    ADD_EXECUTABLE(${TEST} ${TEST}.c)
    TARGET_LINK_LIBRARIES(${TEST} fm )
    ADD_TEST(${TEST} "${TEST}")
endforeach()

foreach (PROG ${PROGS} )
    ADD_EXECUTABLE(${PROG} ${PROG}.c)
    TARGET_LINK_LIBRARIES(${PROG} fm )
    TARGET_LINK_LIBRARIES(${PROG} cod )
    TARGET_LINK_LIBRARIES(${PROG} ffs )
    TARGET_LINK_LIBRARIES(${PROG} cercs_env )
endforeach()

SET(PROGS_INSTALL)
# prefix and suffix elements
foreach(l ${PROGS})
   list(APPEND PROGS_INSTALL ${CMAKE_CURRENT_BINARY_DIR}/${l} )
endforeach()

INSTALL(PROGRAMS ${PROGS_INSTALL} DESTINATION bin)
