#
#  CERCS_ENV_FOUND - system has the Cercs_Env library CERCS_ENV_INCLUDE_DIR - the Cercs_Env include directory CERCS_ENV_LIBRARIES - The libraries needed
#  to use Cercs_Env

set (CERCS_PROJECT "cercs_env")

IF (NOT DEFINED CERCS_ENV_FOUND)
    if (NOT (DEFINED CercsArch))
        execute_process(COMMAND cercs_arch OUTPUT_VARIABLE CercsArch ERROR_QUIET OUTPUT_STRIP_TRAILING_WHITESPACE)
	MARK_AS_ADVANCED(CercsArch)
    endif()
    IF(EXISTS /users/c/chaos)
        FIND_LIBRARY  (CERCS_ENV_LIBRARIES NAMES cercs_env PATHS /users/c/chaos/lib /users/c/chaos/${CercsArch}/${CERCS_PROJECT}/lib /users/c/chaos/${CercsArch}/lib NO_DEFAULT_PATH)
	FIND_PATH (CERCS_ENV_INCLUDE_DIR NAMES cercs_env.h PATHS /users/c/chaos/include /users/c/chaos/${CercsArch}/${CERCS_PROJECT}/include /users/c/chaos/${CercsArch}/include NO_DEFAULT_PATH)
    ENDIF()
    message (STATUS "Use installed is ${USE_INSTALLED}")
    if (NOT (DEFINED USE_INSTALLED) )
    message (STATUS "searching  $ENV{HOME}/include")
	FIND_LIBRARY (CERCS_ENV_LIBRARIES HINTS $ENV{HOME}/lib $ENV{HOME}/${CercsArch}/lib NAMES cercs_env)
	FIND_PATH (CERCS_ENV_INCLUDE_DIR  HINTS $ENV{HOME}/include $ENV{HOME}/${CercsArch}/include NAMES cercs_env.h )
    endif()
    message (STATUS "found ${CERCS_ENV_INCLUDE_DIR}")
    FIND_LIBRARY (CERCS_ENV_LIBRARIES HINTS $ENV{HOME}/lib $ENV{HOME}/${CercsArch}/lib NAMES cercs_env)
    FIND_PATH (CERCS_ENV_INCLUDE_DIR  HINTS $ENV{HOME}/include NAMES cercs_env.h)
    IF (DEFINED CERCS_ENV_LIBRARIES)
	get_filename_component ( CERCS_ENV_LIB_DIR ${CERCS_ENV_LIBRARIES} PATH)
    ENDIF()
    include(FindPackageHandleStandardArgs)
    find_package_handle_standard_args(cercs_env DEFAULT_MSG
     CERCS_ENV_LIBRARIES
     CERCS_ENV_INCLUDE_DIR
     CERCS_ENV_LIB_DIR
   )
   if ((DEFINED CERCS_ENV_LIBRARIES) AND (DEFINED CERCS_ENV_INCLUDE_DIR)) 
   message (STATUS "cercs env found!")
	set(CERCS_ENV_FOUND TRUE)
   endif()
ENDIF()
