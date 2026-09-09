#----------------------------------------------------------------
# Generated CMake target import file.
#----------------------------------------------------------------

# Commands may need to know the format version.
set(CMAKE_IMPORT_FILE_VERSION 1)

# Import target "FSMgine::FSMgine" for configuration ""
set_property(TARGET FSMgine::FSMgine APPEND PROPERTY IMPORTED_CONFIGURATIONS NOCONFIG)
set_target_properties(FSMgine::FSMgine PROPERTIES
  IMPORTED_LINK_INTERFACE_LANGUAGES_NOCONFIG "CXX"
  IMPORTED_LOCATION_NOCONFIG "${_IMPORT_PREFIX}/lib/libFSMgine.a"
  )

list(APPEND _cmake_import_check_targets FSMgine::FSMgine )
list(APPEND _cmake_import_check_files_for_FSMgine::FSMgine "${_IMPORT_PREFIX}/lib/libFSMgine.a" )

# Commands beyond this point should not need to know the version.
set(CMAKE_IMPORT_FILE_VERSION)
