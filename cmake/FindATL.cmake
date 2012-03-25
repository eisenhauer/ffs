#
#  ATL_FOUND - system has the Atl library ATL_INCLUDE_DIR - the Atl include directory ATL_LIBRARIES - The libraries needed
#  to use Atl

set (CERCS_PROJECT "atl")
set (CERCS_PROJECT_INCLUDE "atl.h")

IF (NOT DEFINED ${CERCS_PROJECT}_FOUND)
    if (NOT (DEFINED CercsArch))
        execute_process(COMMAND cercs_arch OUTPUT_VARIABLE CercsArch ERROR_QUIET OUTPUT_STRIP_TRAILING_WHITESPACE)
	MARK_AS_ADVANCED(CercsArch)
    endif()
    IF(EXISTS /users/c/chaos)
        FIND_LIBRARY  (ATL_LIBRARIES NAMES atl PATHS /users/c/chaos/lib /users/c/chaos/${CercsArch}/${CERCS_PROJECT}/lib /users/c/chaos/${CercsArch}/lib NO_DEFAULT_PATH)
	FIND_PATH (ATL_INCLUDE_DIR NAMES ${CERCS_PROJECT_INCLUDE} PATHS /users/c/chaos/include /users/c/chaos/${CercsArch}/${CERCS_PROJECT}/include /users/c/chaos/${CercsArch}/include NO_DEFAULT_PATH)
    ENDIF()
    message (STATUS "Use installed is ${USE_INSTALLED}")
    if (NOT (DEFINED USE_INSTALLED) )
    message (STATUS "searching  $ENV{HOME}/include")
	FIND_LIBRARY (ATL_LIBRARIES HINTS $ENV{HOME}/lib $ENV{HOME}/${CercsArch}/lib NAMES atl)
	FIND_PATH (ATL_INCLUDE_DIR  HINTS $ENV{HOME}/include $ENV{HOME}/${CercsArch}/include NAMES ${CERCS_PROJECT_INCLUDE} )
    endif()
    message (STATUS "found ${ATL_INCLUDE_DIR}")
    FIND_LIBRARY (ATL_LIBRARIES HINTS $ENV{HOME}/lib $ENV{HOME}/${CercsArch}/lib NAMES atl)
    FIND_PATH (ATL_INCLUDE_DIR  HINTS $ENV{HOME}/include NAMES ${CERCS_PROJECT_INCLUDE} )
    IF (DEFINED ATL_LIBRARIES)
	get_filename_component ( ATL_LIB_DIR ${ATL_LIBRARIES} PATH)
    ENDIF()
    include(FindPackageHandleStandardArgs)
    find_package_handle_standard_args(${CERCS_PROJECT} DEFAULT_MSG
     ATL_LIBRARIES
     ATL_INCLUDE_DIR
     ATL_LIB_DIR
   )
   if ((DEFINED ATL_LIBRARIES) AND (DEFINED ATL_INCLUDE_DIR)) 
   message (STATUS "${CERCS_PROJECT} found!")
	set(${CERCS_PROJECT}_FOUND TRUE)
   endif()
ENDIF()
