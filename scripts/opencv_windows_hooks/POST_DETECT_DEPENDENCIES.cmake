# ChromascopeIP M1.6: Normalize OpenCV 5.1.0 MSVC link dependencies.
# This upstream-supported hook executes after dependency discovery but before
# OpenCV creates module targets. It does not modify upstream source files.
# MSVC uses native Windows concurrency; POSIX pthread is not an MSVC library.
if(NOT WIN32 OR NOT MSVC)
  message(FATAL_ERROR "ChromascopeIP OpenCV pthread hook is Windows/MSVC-only")
endif()

list(FIND OPENCV_LINKER_LIBS pthread _chromascope_pthread_index)
if(NOT _chromascope_pthread_index EQUAL -1)
  list(REMOVE_ITEM OPENCV_LINKER_LIBS pthread)
  message(STATUS "ChromascopeIP: removed invalid MSVC pthread link dependency")
else()
  message(STATUS "ChromascopeIP: no global pthread link dependency to remove")
endif()

list(FIND OPENCV_LINKER_LIBS pthread _chromascope_pthread_index)
if(NOT _chromascope_pthread_index EQUAL -1)
  message(FATAL_ERROR "OpenCV MSVC linker list still contains pthread")
endif()
unset(_chromascope_pthread_index)
