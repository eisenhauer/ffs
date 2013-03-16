cmake_minimum_required(VERSION 2.8)

set (TESTS context_test context_test2 fortran_test get_set_test)
set (PROGS ffs_write ffs_index_test ffs_file_test test_file_unique)

ENABLE_TESTING()

INCLUDE_DIRECTORIES(BEFORE ${ffs_BINARY_DIR} ${ffs_SOURCE_DIR}${ffs_SOURCE_DIR}/fm ${ffs_BINARY_DIR}/fm)

foreach (TEST ${TESTS} )
    ADD_EXECUTABLE(${TEST} ${TEST}.c test_funcs.c)
    TARGET_LINK_LIBRARIES(${TEST} ffs)
    ADD_TEST(${TEST} "${TEST}")
endforeach()

ADD_EXECUTABLE(marshal_test marshal_test.c)
TARGET_LINK_LIBRARIES(marshal_test ffs)
ADD_TEST(marshal_test marshal_test)

ADD_EXECUTABLE(marshal_test2 marshal_test2.c)
TARGET_LINK_LIBRARIES(marshal_test2 ffs)
ADD_TEST(marshal_test2 "marshal_test2")

foreach (PROG ${PROGS} )
    ADD_EXECUTABLE(${PROG} ${PROG}.c test_funcs.c)
    TARGET_LINK_LIBRARIES(${PROG} ffs)
endforeach()

if (NOT (DEFINED CercsArch))
    execute_process(COMMAND cercs_arch OUTPUT_VARIABLE CercsArch ERROR_QUIET OUTPUT_STRIP_TRAILING_WHITESPACE)
    MARK_AS_ADVANCED(CercsArch)
endif()

FILE(GLOB CTOS ${ffs_SOURCE_DIR}/ffs/output_dir/context_test_output*.bin)
FILE(GLOB FOS ${ffs_SOURCE_DIR}/ffs/output_dir/file_output*.bin)

ADD_TEST(write_context_output context_test -w ${ffs_BINARY_DIR}/ffs/tests/context_test_output.bin)
ADD_TEST(context_output_unique test_file_unique ${ffs_BINARY_DIR}/ffs/tests/context_test_output.bin ${CTOS} )
ADD_TEST(write_file_output ffs_write ${ffs_BINARY_DIR}/ffs/tests/file_output.bin)
ADD_TEST(file_output_unique test_file_unique ${ffs_BINARY_DIR}/ffs/tests/file_output.bin ${FOS} )
LIST(APPEND CTOS context_test_output.bin)
LIST(REMOVE_DUPLICATES CTOS)
LIST(APPEND FOS file_output.bin)
LIST(REMOVE_DUPLICATES FOS)

ADD_TEST(write_index_output ffs_write -i index_output)
ADD_TEST(read_index_test ffs_index_test index_output)
set_property(TEST read_index_test APPEND PROPERTY DEPENDS write_index_output)

foreach	(CTOUT ${CTOS})
    ADD_TEST(CTOUT_${CTOUT} context_test -r ${CTOUT})
    set_property(TEST CTOUT_${CTOUT} APPEND PROPERTY DEPENDS write_context_output)
endforeach()

foreach	(FOUT ${FOS})
    ADD_TEST(FOUT_${FOUT} ffs_file_test ${FOUT})
    set_property(TEST FOUT_${FOUT} APPEND PROPERTY DEPENDS write_file_output)
endforeach()

