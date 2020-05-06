# - Find nana
# Find the native Nana headers and libraries.
#
#  NANA_INCLUDE_DIRS - where to find nana/nana.h, etc.
#  NANA_LIBRARIES    - List of libraries when using nana.
#  NANA_FOUND        - True if nana found.


# Look for the header file.
FIND_PATH(NANA_INCLUDE_DIR NAMES nana/gui/dragger.hpp)
MARK_AS_ADVANCED(NANA_INCLUDE_DIR)

# Look for the library.
FIND_LIBRARY(NANA_LIBRARY NAMES nana)
MARK_AS_ADVANCED(NANA_LIBRARY)

# handle the QUIETLY and REQUIRED arguments and set NANA_FOUND to TRUE if 
# all listed variables are TRUE
INCLUDE(FindPackageHandleStandardArgs)
FIND_PACKAGE_HANDLE_STANDARD_ARGS(Nana DEFAULT_MSG NANA_LIBRARY NANA_INCLUDE_DIR)

IF(NANA_FOUND)
  SET(NANA_LIBRARIES ${NANA_LIBRARY})
  SET(NANA_INCLUDE_DIRS ${NANA_INCLUDE_DIR})
ELSE(NANA_FOUND)
  SET(NANA_LIBRARIES)
  SET(NANA_INCLUDE_DIRS)
ENDIF(NANA_FOUND)
