# - Try to find the GLEW library
# Once done this will define
#
#  GLEW_FOUND - system has GLEW
#  GLEW_INCLUDE_DIR - the GLEW include directory
#  GLEW_LIBRARIES - The libraries needed to use GLEW

if(GLEW_INCLUDE_DIR AND GLEW_LIBRARIES)
   set(GLEW_FOUND TRUE)
else(GLEW_INCLUDE_DIR AND GLEW_LIBRARIES)

FIND_PATH(GLEW_INCLUDE_DIR NAMES GL/glew.h
   PATHS
   /usr/include
   /usr/local/include
   $ENV{INCLUDE}
   $ENV{GLEWROOT}/include
   $ENV{GLEW_ROOT}/include
   $ENV{GLEW_DIR}/include
   $ENV{GLEW_DIR}/inc
)

FIND_LIBRARY(GLEW_LIBRARIES NAMES glew64 GLEW glew glew32
   PATHS
   /usr/lib
   /usr/lib64
   /usr/local/lib
   /usr/local/lib64
   $ENV{LIBPATH}
   $ENV{GLEWROOT}/lib
   $ENV{GLEW_ROOT}/lib
   $ENV{GLEW_DIR}/lib
   DOC "glew library name"
)

if(GLEW_INCLUDE_DIR AND GLEW_LIBRARIES)
   set(GLEW_FOUND TRUE)
endif(GLEW_INCLUDE_DIR AND GLEW_LIBRARIES)


if(GLEW_FOUND)
   if(NOT GLEW_FIND_QUIETLY)
      message(STATUS "Found GLEW: ${GLEW_LIBRARIES}")
   endif(NOT GLEW_FIND_QUIETLY)
else(GLEW_FOUND)
   if(GLEW_FIND_REQUIRED)
      message(FATAL_ERROR "could NOT find glew")
   endif(GLEW_FIND_REQUIRED)
endif(GLEW_FOUND)

MARK_AS_ADVANCED(GLEW_INCLUDE_DIR GLEW_LIBRARIES)

endif(GLEW_INCLUDE_DIR AND GLEW_LIBRARIES)
