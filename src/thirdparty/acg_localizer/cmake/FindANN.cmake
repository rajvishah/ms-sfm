# - Try to find ANN
# Once done this will define
#  
# ANN_FOUND           - system has UTIL
# ANN_INCLUDE_DIR - theUTIL include directory
# ANN_LIBRARY         - Link these to use UTIL
# ANN_LIBRARY_DIR  - Library DIR of UTIL
#   

IF (ANN_INCLUDE_DIR)
 # Already in cache, be silent
 SET(ANN_FIND_QUIETLY TRUE)
ENDIF (ANN_INCLUDE_DIR)

if( UNIX )
  set( LIBDIR lib )
elseif( APPLE )
  set( LIBDIR lib )
endif()


FIND_PATH(ANN_INCLUDE_DIR ANN/ANN.h
	  PATHS "/home/cvit/rajvi/local_libs/include/"
                )

if( ANN_INCLUDE_DIR )
   set( ANN_FOUND TRUE )

   set( ANN_LIBRARY_DIR "/home/cvit/rajvi/local_libs/lib/" )

   set( ANN_LIBRARY optimized ANN debug ANN )

ELSE (ANN_INCLUDE_DIR)
   SET(ANN_FOUND FALSE )
ENDIF (ANN_INCLUDE_DIR )

