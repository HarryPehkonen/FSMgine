#----------------------------------------------------------------
# Generated CMake target import file.
#----------------------------------------------------------------

# Commands may need to know the format version.
set(CMAKE_IMPORT_FILE_VERSION 1)

# Import target "FSMgine::FSMgineMT" for configuration ""
set_property(TARGET FSMgine::FSMgineMT APPEND PROPERTY IMPORTED_CONFIGURATIONS NOCONFIG)
set_target_properties(FSMgine::FSMgineMT PROPERTIES
  IMPORTED_LINK_INTERFACE_LANGUAGES_NOCONFIG "CXX"
  IMPORTED_LOCATION_NOCONFIG "${_IMPORT_PREFIX}/lib/libFSMgineMT.a"
  )

list(APPEND _cmake_import_check_targets FSMgine::FSMgineMT )
list(APPEND _cmake_import_check_files_for_FSMgine::FSMgineMT "${_IMPORT_PREFIX}/lib/libFSMgineMT.a" )

# Commands beyond this point should not need to know the version.
set(CMAKE_IMPORT_FILE_VERSION)
