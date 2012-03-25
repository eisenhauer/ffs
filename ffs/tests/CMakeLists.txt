cmake_minimum_required(VERSION 2.8)

set (TESTS context_test context_test2 fortran_test)
set (PROGS ffs_write ffs_index_test ffs_file_test)

ENABLE_TESTING()

INCLUDE_DIRECTORIES(BEFORE ${ffs_BINARY_DIR} ${ffs_SOURCE_DIR}${ffs_SOURCE_DIR}/fm ${ffs_BINARY_DIR}/fm)

foreach (TEST ${TESTS} )
    ADD_EXECUTABLE(${TEST} ${TEST}.c test_funcs.c)
    TARGET_LINK_LIBRARIES(${TEST} fm cod ffs)
    ADD_TEST(${TEST} "${TEST}")
endforeach()

ADD_EXECUTABLE(marshal_test marshal_test.c)
TARGET_LINK_LIBRARIES(marshal_test fm cod ffs)
ADD_TEST(marshal_test marshal_test)

ADD_EXECUTABLE(marshal_test2 marshal_test2.c)
TARGET_LINK_LIBRARIES(marshal_test2 fm cod ffs)
ADD_TEST(marshal_test2 "marshal_test2")

foreach (PROG ${PROGS} )
    ADD_EXECUTABLE(${PROG} ${PROG}.c test_funcs.c)
    TARGET_LINK_LIBRARIES(${PROG} fm cod ffs)
endforeach()

if (NOT (DEFINED CercsArch))
    execute_process(COMMAND cercs_arch OUTPUT_VARIABLE CercsArch ERROR_QUIET OUTPUT_STRIP_TRAILING_WHITESPACE)
    MARK_AS_ADVANCED(CercsArch)
endif()

FILE(GLOB CTOS ${ffs_SOURCE_DIR}/ffs/output_dir/context_test_output*.bin)
FILE(GLOB FOS ${ffs_SOURCE_DIR}/ffs/output_dir/file_output*.bin)
if (CercsArch)
    ADD_TEST(write_context_output context_test -w ${ffs_SOURCE_DIR}/ffs/output_dir/context_test_output.${CercsArch}.bin)
    ADD_TEST(write_file_output ffs_write ${ffs_SOURCE_DIR}/ffs/output_dir/file_output.${CercsArch}.bin)
    LIST(APPEND CTOS ${ffs_SOURCE_DIR}/ffs/output_dir/context_test_output.${CercsArch}.bin)
    LIST(REMOVE_DUPLICATES CTOS)
    LIST(APPEND FOS ${ffs_SOURCE_DIR}/ffs/output_dir/file_output.${CercsArch}.bin)
    LIST(REMOVE_DUPLICATES FOS)
endif()

ADD_TEST(write_index_output ffs_write -i index_output)
ADD_TEST(read_index_test ffs_index_test index_output)

foreach	(CTOUT ${CTOS})
    ADD_TEST(CTOUT_${CTOUT} context_test -r ${CTOUT})
endforeach()

foreach	(FOUT ${FOS})
    ADD_TEST(FOUT_${FOUT} ffs_file_test ${FOUT})
endforeach()

