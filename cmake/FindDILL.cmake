#
#  DILL_FOUND - system has the Dill library DILL_INCLUDE_DIR - the Dill include directory DILL_LIBRARIES - The libraries needed
#  to use Dill

set (CERCS_LIB "dill")
string(TOUPPER ${CERCS_LIB} CERCS_PROJECT)
string(TOLOWER ${CERCS_LIB} CERCS_PROJECT_DIR)
set (CERCS_PROJECT_INCLUDE "dill.h")

IF (NOT DEFINED ${CERCS_PROJECT}_FOUND)
    if (NOT (DEFINED CercsArch))
        execute_process(COMMAND cercs_arch OUTPUT_VARIABLE CercsArch ERROR_QUIET OUTPUT_STRIP_TRAILING_WHITESPACE)
	MARK_AS_ADVANCED(CercsArch)
    endif()
    IF(EXISTS /users/c/chaos)
        FIND_LIBRARY  (${CERCS_PROJECT}_LIBRARIES NAMES ${CERCS_LIB} PATHS /users/c/chaos/lib /users/c/chaos/${CercsArch}/${CERCS_PROJECT_DIR}/lib /users/c/chaos/${CercsArch}/lib NO_DEFAULT_PATH)
	FIND_PATH (${CERCS_PROJECT}_INCLUDE_DIR NAMES ${CERCS_PROJECT_INCLUDE} PATHS /users/c/chaos/include /users/c/chaos/${CercsArch}/${CERCS_PROJECT_DIR}/include /users/c/chaos/${CercsArch}/include NO_DEFAULT_PATH)
    ENDIF()
    message (STATUS "Use installed is ${USE_INSTALLED}")
    if (NOT (DEFINED USE_INSTALLED) )
    message (STATUS "searching  $ENV{HOME}/include")
	FIND_LIBRARY (${CERCS_PROJECT}_LIBRARIES HINTS $ENV{HOME}/lib $ENV{HOME}/${CercsArch}/lib NAMES ${CERCS_LIB})
	FIND_PATH (${CERCS_PROJECT}_INCLUDE_DIR  HINTS $ENV{HOME}/include $ENV{HOME}/${CercsArch}/include NAMES ${CERCS_PROJECT_INCLUDE} )
    endif()
    message (STATUS "found ${${CERCS_PROJECT}_INCLUDE_DIR}")
    FIND_LIBRARY (${CERCS_PROJECT}_LIBRARIES HINTS $ENV{HOME}/lib $ENV{HOME}/${CercsArch}/lib NAMES ${CERCS_LIB})
    FIND_PATH (${CERCS_PROJECT}_INCLUDE_DIR  HINTS $ENV{HOME}/include NAMES ${CERCS_PROJECT_INCLUDE} )
    IF (DEFINED ${CERCS_PROJECT}_LIBRARIES)
	get_filename_component ( ${CERCS_PROJECT}_LIB_DIR ${${CERCS_PROJECT}_LIBRARIES} PATH)
    ENDIF()
    include(FindPackageHandleStandardArgs)
    find_package_handle_standard_args(${CERCS_PROJECT} DEFAULT_MSG
     ${CERCS_PROJECT}_LIBRARIES
     ${CERCS_PROJECT}_INCLUDE_DIR
     ${CERCS_PROJECT}_LIB_DIR
   )
   if ((DEFINED ${CERCS_PROJECT}_LIBRARIES) AND (DEFINED ${CERCS_PROJECT}_INCLUDE_DIR)) 
   message (STATUS "${CERCS_PROJECT} found!")
   message (STATUS "${CERCS_PROJECT}_LIBRARIES is    ${${CERCS_PROJECT}_LIBRARIES}")
	set(${CERCS_PROJECT}_FOUND TRUE)
   endif()
ENDIF()
