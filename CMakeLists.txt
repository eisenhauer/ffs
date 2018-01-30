cmake_minimum_required(VERSION 2.8.3)
cmake_policy(VERSION 2.8.3)
project(ffs)

IF( NOT CMAKE_BUILD_TYPE )
SET( CMAKE_BUILD_TYPE "RelWithDebInfo" )
ENDIF()

set(CMAKE_MODULE_PATH ${CMAKE_MODULE_PATH} ${CMAKE_CURRENT_SOURCE_DIR}/cmake)

include(CheckFunctionExists)
include(CheckIncludeFiles)
include(CheckTypeSize)
include(CheckStructHasMember)
include(CMakeFindDependencyMacro)

set(FFS_MAJOR_VERSION 1)
set(FFS_MINOR_VERSION 5)
set(FFS_PATCH_VERSION 0)
set(FFS_VERSION
  ${FFS_MAJOR_VERSION}.${FFS_MINOR_VERSION}.${FFS_PATCH_VERSION})

# Offer the user the choice of overriding the installation directories
set(INSTALL_LIB_DIR lib CACHE PATH "Installation directory for libraries")
set(INSTALL_BIN_DIR bin CACHE PATH "Installation directory for executables")
set(INSTALL_INCLUDE_DIR include CACHE PATH
  "Installation directory for header files")
if(WIN32 AND NOT CYGWIN)
  set(DEF_INSTALL_CMAKE_DIR CMake)
else()
  set(DEF_INSTALL_CMAKE_DIR lib/CMake/ffs)
endif()
set(INSTALL_CMAKE_DIR ${DEF_INSTALL_CMAKE_DIR} CACHE PATH
  "Installation directory for CMake files")
 
# Make relative paths absolute (needed later on)
foreach(p LIB BIN INCLUDE CMAKE)
  set(var INSTALL_${p}_DIR)
  if(NOT IS_ABSOLUTE "${${var}}")
    set(${var} "${CMAKE_INSTALL_PREFIX}/${${var}}")
  endif()
endforeach()
 
CHECK_TYPE_SIZE("double" SIZEOF_DOUBLE)
CHECK_TYPE_SIZE("float" SIZEOF_FLOAT)
CHECK_TYPE_SIZE("int" SIZEOF_INT)
CHECK_TYPE_SIZE("off_t" SIZEOF_OFF_T)
CHECK_TYPE_SIZE("long" SIZEOF_LONG)
CHECK_TYPE_SIZE("long double" SIZEOF_LONG_DOUBLE)
CHECK_TYPE_SIZE("long long" SIZEOF_LONG_LONG)
CHECK_TYPE_SIZE("size_t" SIZEOF_SIZE_T)

# use, i.e. don't skip the full RPATH for the build tree
SET(CMAKE_SKIP_BUILD_RPATH  FALSE)

# when building, don't use the install RPATH already
# (but later on when installing)
SET(CMAKE_BUILD_WITH_INSTALL_RPATH FALSE) 

SET(CMAKE_INSTALL_RPATH "${CMAKE_INSTALL_PREFIX}/lib")

# add the automatically determined parts of the RPATH
# which point to directories outside the build tree to the install RPATH
SET(CMAKE_INSTALL_RPATH_USE_LINK_PATH TRUE)


# the RPATH to be used when installing, but only if it's not a system directory
LIST(FIND CMAKE_PLATFORM_IMPLICIT_LINK_DIRECTORIES "${CMAKE_INSTALL_PREFIX}/lib" isSystemDir)
IF("${isSystemDir}" STREQUAL "-1")
   SET(CMAKE_INSTALL_RPATH "${CMAKE_INSTALL_PREFIX}/lib")
ENDIF("${isSystemDir}" STREQUAL "-1")

set (CMAKE_MACOSX_RPATH 1)

INCLUDE_DIRECTORIES(${CMAKE_CURRENT_BINARY_DIR} ${CMAKE_CURRENT_SOURCE_DIR}
${CMAKE_CURRENT_SOURCE_DIR}/fm ${CMAKE_CURRENT_SOURCE_DIR}/cod
${CMAKE_CURRENT_SOURCE_DIR}/ffs ${CMAKE_CURRENT_BINARY_DIR}/ffs)

if (NOT (DEFINED NO_SOCKETS))
    check_function_exists(socket SOCKETS_FOUND)
    if (NOT SOCKETS_FOUND)
	set (NO_SOCKETS true)
    endif()
endif()
if (NO_SOCKETS) 
    set (FM_SOCKET_IO null_io.c)
elseif (WIN32)
    set (FM_SOCKET_IO server_acts.c nt_io.c)
else ()
    set (FM_SOCKET_IO server_acts.c unix_io.c)
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

if (NOT BISON_FOUND OR NOT FLEX_FOUND) 
   include ( BisonFlexSub )
   SETUP_BISON_FLEX_SUB ()
else()
   BISON_TARGET(CODParser cod/cod.y ${CMAKE_CURRENT_BINARY_DIR}/cod.tab.c COMPILE_FLAGS -d)
   set (HAVE_COD_PARSER_H)
   FLEX_TARGET(CODScanner cod/cod.l  ${CMAKE_CURRENT_BINARY_DIR}/lex.yy.c)
   ADD_FLEX_BISON_DEPENDENCY(CODScanner CODParser)
endif ()
add_custom_command(
  OUTPUT "cod_node.c" "structs.h"
  COMMAND perl ${CMAKE_CURRENT_SOURCE_DIR}/cod/struct.pl ${CMAKE_CURRENT_SOURCE_DIR}/cod/cod.structs
  DEPENDS lex.yy.c ${CMAKE_CURRENT_SOURCE_DIR}/cod/cod.structs ${CMAKE_CURRENT_SOURCE_DIR}/cod/struct.pl
)

IF (TARGET_CNL)   # This should be set to true if compiling for compute note linux
  set (NO_DYNAMIC_LINKING 1)   # dummy dyn linking in cm.c and cm_transport.c
ELSE ()
  FIND_PACKAGE(DL)
ENDIF ()

add_custom_target(
    docs ALL
    DEPENDS "cod_node.c"
    )

add_custom_target (generated
		  DEPENDS cod_node.c ${BISON_CODParser_OUTPUT_SOURCE} )

LIST (APPEND COD_MASTER_SRC_LIST ${BISON_CODParser_OUTPUT_SOURCE} ${CMAKE_CURRENT_BINARY_DIR}/cod_node.c)

IF( NOT CMAKE_BUILD_TYPE )
    SET( CMAKE_BUILD_TYPE "RelWithDebInfo" )
ENDIF()

set (FFS_MASTER_SRC_LIST ${FFS_MASTER_SRC_LIST} ${FM_MASTER_SRC_LIST} ${COD_MASTER_SRC_LIST})

set (CMAKE_PREFIX_PATH "${CMAKE_INSTALL_PREFIX}/lib/CMake")
find_dependency(atl REQUIRED)
find_dependency(dill REQUIRED)

list (APPEND INC_DIRS ${ATL_INCLUDE_DIRS} ${DILL_INCLUDE_DIRS} ${DL_INCLUDE_DIRS})
list (APPEND BASE_LIBS ${ATL_LIBRARIES} ${DILL_LIBRARIES} ${DL_LIBRARIES})

find_library(M_LIB m)
if (NOT ${M_LIB} STREQUAL "M_LIB-NOTFOUND")
  list (APPEND BASE_LIBS "-lm")
endif()

add_library(ffs ${FFS_MASTER_SRC_LIST})
target_link_libraries(ffs ${BASE_LIBS})
set_target_properties(ffs PROPERTIES VERSION ${FFS_VERSION_STRING}
                                          SOVERSION ${FFS_VERSION_MAJOR})

INCLUDE_DIRECTORIES(${INC_DIRS})

IF (dill_FOUND)
    set (HAVE_DILL_H ON)
    SET(CMAKE_REQUIRED_LIBRARIES "${dill_LIBRARIES}")
    check_function_exists(dill_native_dcg DILL_HAS_NATIVE_DCG)
    if (NOT DILL_HAS_NATIVE_DCG)
       SET(FFS_CONVERSION_GENERATION_DEFAULT FALSE)
    else ()
       SET(FFS_CONVERSION_GENERATION_DEFAULT TRUE)
    endif()
    UNSET(CMAKE_REQUIRED_LIBRARIES)
    set (DO_DCG TRUE)
ENDIF()
if (atl_FOUND)
  set (HAVE_ATL_H ON)
endif(atl_FOUND)

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


if (${SIZEOF_SIZE_T} EQUAL ${SIZEOF_INT}) 
   set (UIO_SIZE_T_TYPE "unsigned int")
else ()
   set (UIO_SIZE_T_TYPE "unsigned long")
endif()

configure_file(ffs/ffs.h.in ffs/ffs.h @ONLY)
configure_file(CTestCustom.ctest.in CTestCustom.ctest @ONLY)

if ("${SIZEOF_LONG}" EQUAL 8) 
   set (DATA_LEN_TYPE "long")
elseif ("${SIZEOF_LONG_LONG}" EQUAL 8)
   set (DATA_LEN_TYPE "long long")
else ()
   set (DATA_LEN_TYPE "undefined")
endif()

CHECK_FUNCTION_EXISTS(strtof HAVE_STRTOF)
CHECK_FUNCTION_EXISTS(strtod HAVE_STRTOD)
CHECK_FUNCTION_EXISTS(strtold HAVE_STRTOLD)
CHECK_FUNCTION_EXISTS(getdomainname HAVE_GETDOMAINNAME)

set(CMAKE_EXTRA_INCLUDE_FILES sys/socket.h)
CHECK_TYPE_SIZE("struct iovec" IOVEC_DEFINE)
unset(CMAKE_EXTRA_INCLUDE_FILES)
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

install(TARGETS ffs
  EXPORT ffsTargets
  RUNTIME DESTINATION "${INSTALL_BIN_DIR}" COMPONENT bin
  LIBRARY DESTINATION "${INSTALL_LIB_DIR}" COMPONENT shlib
  ARCHIVE DESTINATION "${INSTALL_LIB_DIR}" COMPONENT lib
  PUBLIC_HEADER DESTINATION "${INSTALL_INCLUDE_DIR}/ffs"
    COMPONENT dev)

if (${CMAKE_C_COMPILER_ID} MATCHES "Intel") 
    set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -shared-intel")
endif()

CONFIGURE_FILE( ${CMAKE_CURRENT_SOURCE_DIR}/config.h.cmake ${CMAKE_CURRENT_BINARY_DIR}/config.h )

# Add all targets to the build-tree export set
export(TARGETS ffs
  FILE "${PROJECT_BINARY_DIR}/ffsTargets.cmake")
 
# Export the package for use from the build-tree
# (this registers the build-tree with a global CMake-registry)
export(PACKAGE ffs)
 
# Create the ffsConfig.cmake and ffsConfigVersion files
file(RELATIVE_PATH REL_INCLUDE_DIR "${INSTALL_CMAKE_DIR}"
   "${INSTALL_INCLUDE_DIR}")
# ... for the build tree
set(CONF_INCLUDE_DIRS "${PROJECT_SOURCE_DIR}" "${PROJECT_BINARY_DIR}")
configure_file(ffsConfig.cmake.in
  "${PROJECT_BINARY_DIR}/ffsConfig.cmake" @ONLY)
# ... for the install tree
set(CONF_INCLUDE_DIRS "\${FFS_CMAKE_DIR}/${REL_INCLUDE_DIR}")
configure_file(ffsConfig.cmake.in
  "${PROJECT_BINARY_DIR}${CMAKE_FILES_DIRECTORY}/ffsConfig.cmake" @ONLY)
# ... for both
configure_file(ffsConfigVersion.cmake.in
  "${PROJECT_BINARY_DIR}/ffsConfigVersion.cmake" @ONLY)
 
# Install the ffsConfig.cmake and ffsConfigVersion.cmake
install(FILES
  "${PROJECT_BINARY_DIR}${CMAKE_FILES_DIRECTORY}/ffsConfig.cmake"
  "${PROJECT_BINARY_DIR}/ffsConfigVersion.cmake"
  DESTINATION "${INSTALL_CMAKE_DIR}" COMPONENT dev)
 
# Install the export set for use with the install-tree
install(EXPORT ffsTargets DESTINATION
  "${INSTALL_CMAKE_DIR}" COMPONENT dev)

# display status message for important variables
MESSAGE( STATUS )
MESSAGE( STATUS "-------------------------------------------------------------------------------" )
MESSAGE( STATUS "CMAKE_INSTALL_PREFIX = ${CMAKE_INSTALL_PREFIX}" )
MESSAGE( STATUS "CMAKE_BUILD_TYPE = ${CMAKE_BUILD_TYPE}  (options are: Debug,Release,RelWithDebInfo,MinSizeRel)")
MESSAGE( STATUS "LIBRARIES_ONLY = ${LIBRARIES_ONLY}" )
MESSAGE( STATUS "Change a value with: cmake -D<Variable>=<Value>" )
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
