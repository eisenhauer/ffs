cmake_minimum_required(VERSION 2.8)
project(ffs)

IF( NOT CMAKE_BUILD_TYPE )
SET( CMAKE_BUILD_TYPE "RelWithDebInfo" )
ENDIF()

set(CMAKE_MODULE_PATH ${CMAKE_MODULE_PATH} ${CMAKE_CURRENT_SOURCE_DIR}/cmake)

include(CheckFunctionExists)
include(CheckIncludeFiles)
include(CheckTypeSize)
include(CheckStructHasMember)
include(FindCERCSProject)

INCLUDE_DIRECTORIES(${CMAKE_CURRENT_BINARY_DIR} ${CMAKE_CURRENT_SOURCE_DIR}
${CMAKE_CURRENT_SOURCE_DIR}/fm ${CMAKE_CURRENT_SOURCE_DIR}/cod
${CMAKE_CURRENT_SOURCE_DIR}/ffs)

if (DEFINED CERCS_USE_INSTALLED) 
   set (CERCS_USE_INSTALLED "USE_INSTALLED")
endif(DEFINED CERCS_USE_INSTALLED) 

if (NOT (DEFINED NO_SOCKETS))
    check_function_exists(socket SOCKETS_FOUND)
    if (NOT SOCKETS_FOUND)
	set (NO_SOCKETS true)
    endif()
endif()
if (NO_SOCKETS) 
    set (FM_SOCKET_IO null_io.c)
elseif (WIN32)
    set (FM_SOCKET_IO server_acts.c server.c nt_io.c)
else ()
    set (FM_SOCKET_IO server_acts.c server.c unix_io.c)
endif()

set (FM_SRC_LIST fm_formats.c fm_dump.c lookup3.c string_conversion.c fm_get.c xml.c ${FM_SOCKET_IO})
FOREACH (FM_SRC ${FM_SRC_LIST})
    LIST (APPEND FM_MASTER_SRC_LIST fm/${FM_SRC})
ENDFOREACH()

set (COD_SRC_LIST cg.c standard.c)
FOREACH (COD_SRC ${COD_SRC_LIST})
    LIST (APPEND COD_MASTER_SRC_LIST cod/${COD_SRC})
ENDFOREACH()

set (FFS_SRC_LIST ffs.c ffs_formats.c ffs_conv.c ffs_gen.c ffs_file.c unix_io.c evol.c ffs_marshal.c )
FOREACH (FFS_SRC ${FFS_SRC_LIST})
    LIST (APPEND FFS_MASTER_SRC_LIST ffs/${FFS_SRC})
ENDFOREACH()
LIST(APPEND FFS_MASTER_SRC_LIST version.c)

FIND_PACKAGE (BISON)
FIND_PACKAGE (FLEX)

BISON_TARGET(CODParser cod/cod.y ${CMAKE_CURRENT_BINARY_DIR}/cod.tab.c COMPILE_FLAGS -d)
set (HAVE_COD_PARSER_H)
FLEX_TARGET(CODScanner cod/cod.l  ${CMAKE_CURRENT_BINARY_DIR}/lex.yy.c)
ADD_FLEX_BISON_DEPENDENCY(CODScanner CODParser)
add_custom_command(
  OUTPUT "cod_node.c" "structs.h"
  COMMAND perl ${CMAKE_CURRENT_SOURCE_DIR}/cod/struct.pl ${CMAKE_CURRENT_SOURCE_DIR}/cod/cod.structs
  DEPENDS lex.yy.c ${CMAKE_CURRENT_SOURCE_DIR}/cod/cod.structs ${CMAKE_CURRENT_SOURCE_DIR}/cod/struct.pl
)
LIST (APPEND COD_MASTER_SRC_LIST ${BISON_CODParser_OUTPUT_SOURCE} ${CMAKE_CURRENT_BINARY_DIR}/cod_node.c)

IF( NOT CMAKE_BUILD_TYPE )
    SET( CMAKE_BUILD_TYPE "RelWithDebInfo" )
ENDIF()

set (FFS_MASTER_SRC_LIST ${FFS_MASTER_SRC_LIST} ${FM_MASTER_SRC_LIST} ${COD_MASTER_SRC_LIST})

add_library(ffs SHARED ${FFS_MASTER_SRC_LIST})
add_library(ffs-static STATIC ${FFS_MASTER_SRC_LIST})
# The library target "ffs" already has a default OUTPUT_NAME of "ffs", so we don't need to change it.
# The library target "ffs-static" has a default OUTPUT_NAME of "ffs-static", so change it.
SET_TARGET_PROPERTIES(ffs-static PROPERTIES OUTPUT_NAME "ffs" )
# Now the library target "foo-static" will be named "foo.lib" with MS tools.
# This conflicts with the "foo.lib" import library corresponding to "foo.dll",
# so we add a "lib" prefix (which is default on other platforms anyway):
SET_TARGET_PROPERTIES(ffs-static PROPERTIES PREFIX "lib" LINKER_LANGUAGE C)
SET_TARGET_PROPERTIES(ffs PROPERTIES LINKER_LANGUAGE C)

# add_dependencies(cod fm)
# add_dependencies(ffs cod)

FIND_CERCS_PROJECT (cercs_env LIBRARY cercs_env INCLUDES cercs_env.h REQUIRED ${CERCS_USE_INSTALLED})
FIND_CERCS_PROJECT (atl LIBRARY atl INCLUDES atl.h REQUIRED ${CERCS_USE_INSTALLED})
FIND_CERCS_PROJECT (gen_thread LIBRARY gen_thread ${CERCS_USE_INSTALLED})
FIND_CERCS_PROJECT(dill LIBRARY dill INCLUDES dill.h ${CERCS_USE_INSTALLED})

list (APPEND INC_DIRS ${CERCS_ENV_INCLUDE_DIR} ${ATL_INCLUDE_DIR} ${GEN_THREAD_INCLUDE_DIR} ${DILL_INCLUDE_DIR})
list (APPEND LIB_DIRS ${CERCS_ENV_LIB_DIR} ${ATL_LIB_DIR} ${GEN_THREAD_LIB_DIR} ${DILL_LIB_DIR})
list (APPEND BASE_LIBS ${CERCS_ENV_LIBRARIES} ${ATL_LIBRARIES} ${GEN_THREAD_LIBRARIES} ${DILL_LIBRARIES})

INCLUDE_DIRECTORIES(${INC_DIRS})
LINK_DIRECTORIES(${LIB_DIRS})

TARGET_LINK_LIBRARIES(ffs ${BASE_LIBS})
TARGET_LINK_LIBRARIES(ffs-static ${BASE_LIBS})

IF (DILL_FOUND)
    set (FFS_CONVERSION_GENERATION_DEFAULT TRUE)
    set (DO_DCG TRUE)
ENDIF()

# TARGET_LINK_LIBRARIES(cod fm)
# TARGET_LINK_LIBRARIES(ffs cod)

CHECK_INCLUDE_FILE(malloc.h HAVE_MALLOC_H)
CHECK_INCLUDE_FILE(memory.h HAVE_MEMORY_H)
CHECK_INCLUDE_FILE(netdb.h HAVE_NETDB_H)
CHECK_INCLUDE_FILE(sockLib.h HAVE_SOCKLIB_H)
CHECK_INCLUDE_FILE(stdarg.h STDC_HEADERS)
CHECK_INCLUDE_FILE(stdlib.h HAVE_STDLIB_H)
CHECK_INCLUDE_FILE(string.h HAVE_STRING_H)
CHECK_INCLUDE_FILE(sys/select.h HAVE_SYS_SELECT_H)
CHECK_INCLUDE_FILE(sys/socket.h HAVE_SYS_SOCKET_H)
CHECK_INCLUDE_FILE(sys/time.h HAVE_SYS_TIME_H)
CHECK_INCLUDE_FILE(sys/times.h HAVE_SYS_TIMES_H)
CHECK_INCLUDE_FILE(sys/uio.h HAVE_SYS_UIO_H)
CHECK_INCLUDE_FILE(sys/un.h HAVE_SYS_UN_H)
CHECK_INCLUDE_FILE(unistd.h HAVE_UNISTD_H)
CHECK_INCLUDE_FILE(windows.h HAVE_WINDOWS_H)
CHECK_INCLUDE_FILE(winsock.h HAVE_WINSOCK_H)

CHECK_TYPE_SIZE(double SIZEOF_DOUBLE)
CHECK_TYPE_SIZE(float SIZEOF_FLOAT)
CHECK_TYPE_SIZE(int SIZEOF_INT)
CHECK_TYPE_SIZE(off_t SIZEOF_OFF_T)
CHECK_TYPE_SIZE(long SIZEOF_LONG)
CHECK_TYPE_SIZE("long double" SIZEOF_LONG_DOUBLE)
CHECK_TYPE_SIZE("long long" SIZEOF_LONG_LONG)
CHECK_TYPE_SIZE("size_t" SIZEOF_SIZE_T)

if (${SIZEOF_SIZE_T} EQUAL ${SIZEOF_INT}) 
   set (UIO_SIZE_T_TYPE "unsigned int")
else ()
   set (UIO_SIZE_T_TYPE "unsigned long")
endif()

configure_file(ffs/ffs.h.in ffs/ffs.h @ONLY)

if (${SIZEOF_LONG} EQUAL 8) 
   set (DATA_LEN_TYPE "long")
elseif (${SIZEOF_LONG_LONG} EQUAL 8)
   set (DATA_LEN_TYPE "long long")
else ()
   set (DATA_LEN_TYPE "undefined")
endif()

CHECK_FUNCTION_EXISTS(strtof HAVE_STRTOF)
CHECK_FUNCTION_EXISTS(strtod HAVE_STRTOD)
CHECK_FUNCTION_EXISTS(strtold HAVE_STRTOLD)
CHECK_FUNCTION_EXISTS(getdomainname HAVE_GETDOMAINNAME)
CHECK_STRUCT_HAS_MEMBER("struct iovec" iov_base sys/uio.h HAS_IOV_BASE_IOVEC)

if (NOT ${HAS_IOV_BASE_IOVEC}) 
   set (NEED_IOVEC_DEFINE true)
endif()

IF (NOT DEFINED (FORMAT_SERVER_HOST) )
   set (FORMAT_SERVER_HOST "formathost.cercs.gatech.edu")
ENDIF()

IF (NOT DEFINED (FORMAT_SERVICE_DOMAIN) )
   set (FORMAT_SERVICE_DOMAIN "")
ENDIF()

INSTALL(FILES ${CMAKE_CURRENT_SOURCE_DIR}/fm/fm.h DESTINATION include)
INSTALL(FILES ${CMAKE_CURRENT_SOURCE_DIR}/cod/cod.h DESTINATION include)
INSTALL(FILES ${CMAKE_CURRENT_BINARY_DIR}/ffs/ffs.h DESTINATION include)
INSTALL(TARGETS ffs ffs-static 
  RUNTIME DESTINATION bin
  LIBRARY DESTINATION lib
  ARCHIVE DESTINATION lib
)

if (${CMAKE_C_COMPILER_ID} MATCHES "Intel") 
    set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -shared-intel")
endif()

CONFIGURE_FILE( ${CMAKE_CURRENT_SOURCE_DIR}/config.h.cmake ${CMAKE_CURRENT_BINARY_DIR}/config.h )

# display status message for important variables
MESSAGE( STATUS )
MESSAGE( STATUS "-------------------------------------------------------------------------------" )
MESSAGE( STATUS "CMAKE_INSTALL_PREFIX = ${CMAKE_INSTALL_PREFIX}" )
MESSAGE( STATUS "CMAKE_BUILD_TYPE = ${CMAKE_BUILD_TYPE}"  (options are: Debug,Release,RelWithDebInfo,MinSizeRel))
MESSAGE( STATUS "Change a value with: cmake -D<Variable>=<Value>" )
MESSAGE( STATUS "    to use installed CERCS libraries specify -DCERCS_USE_INSTALLED=1" )
MESSAGE( STATUS "-------------------------------------------------------------------------------" )

INCLUDE(CTest)
ENABLE_TESTING()
IF(BUILD_TESTING)
  SET(BUILDNAME "${BUILDNAME}" CACHE STRING "Name of build on the dashboard")
  MARK_AS_ADVANCED(BUILDNAME)
ENDIF(BUILD_TESTING)

ADD_SUBDIRECTORY( fm )
ADD_SUBDIRECTORY( cod )
ADD_SUBDIRECTORY( ffs )
