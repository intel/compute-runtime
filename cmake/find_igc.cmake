#
# Copyright (C) 2021 Intel Corporation
#
# SPDX-License-Identifier: MIT
#

# Intel Graphics Compiler detection
if(NOT IGC__IGC_TARGETS)
  # check whether igc is part of workspace
  if(DEFINED IGC_DIR)
    get_filename_component(IGC_DIR "${IGC_DIR}" ABSOLUTE)
  else()
    get_filename_component(IGC_DIR_tmp "${NEO_SOURCE_DIR}/../igc" ABSOLUTE)
    if(IS_DIRECTORY "${IGC_DIR_tmp}")
      set(IGC_DIR "${IGC_DIR_tmp}")
    endif()
  endif()

  if(UNIX)
    # on Unix-like use pkg-config
    find_package(PkgConfig)
    if(DEFINED IGC_DIR)
      if(IS_DIRECTORY "${IGC_DIR}/lib/pkgconfig/")
        set(__tmp_LIBDIR "lib")
      elseif(IS_DIRECTORY "${IGC_DIR}/${CMAKE_INSTALL_LIBDIR}/pkgconfig/")
        set(__tmp_LIBDIR ${CMAKE_INSTALL_LIBDIR})
      endif()
    endif()
    if(DEFINED __tmp_LIBDIR)
      set(OLD_PKG_CONFIG_PATH $ENV{PKG_CONFIG_PATH})
      set(ENV{PKG_CONFIG_PATH} "${IGC_DIR}/${__tmp_LIBDIR}/pkgconfig")
    endif()
    pkg_check_modules(NEO__IGC igc-opencl)
    if(DEFINED __tmp_LIBDIR)
      set(ENV{PKG_CONFIG_PATH} ${OLD_PKG_CONFIG_PATH})
      set(NEO__IGC_LIBRARY_PATH "${IGC_DIR}/${__tmp_LIBDIR}/")
    endif()

    if(NEO__IGC_FOUND)
      if(DEFINED IGC_DIR AND IS_DIRECTORY "${IGC_DIR}/${__tmp_LIBDIR}/pkgconfig/")
        string(REPLACE "${NEO__IGC_INCLUDEDIR}" "${IGC_DIR}/include/igc" NEO__IGC_INCLUDE_DIRS "${NEO__IGC_INCLUDE_DIRS}")
      endif()

      set(NEO__IGC_INCLUDE_DIR ${NEO__IGC_INCLUDE_DIRS})
      message(STATUS "IGC include dirs: ${NEO__IGC_INCLUDE_DIR}")
    endif()
    if(DEFINED __tmp_LIBDIR)
      unset(__tmp_LIBDIR)
    endif()
  endif()

  if(NEO__IGC_FOUND)
    # do nothing
  elseif(EXISTS "${IGC_DIR}/CMakeLists.txt")
    message(STATUS "IGC source dir is: ${IGC_DIR}")

    set(IGC_OPTION__OUTPUT_DIR "${NEO_BUILD_DIR}/igc")
    set(IGC_OPTION__INCLUDE_IGC_COMPILER_TOOLS OFF)
    add_subdirectory_unique("${IGC_DIR}" "${NEO_BUILD_DIR}/igc" EXCLUDE_FROM_ALL)

    set(NEO__IGC_TARGETS "${IGC__IGC_TARGETS}")
    foreach(TARGET_tmp ${NEO__IGC_TARGETS})
      list(APPEND NEO__IGC_INCLUDE_DIR $<TARGET_PROPERTY:${TARGET_tmp},INTERFACE_INCLUDE_DIRECTORIES>)
      list(APPEND NEO__IGC_COMPILE_DEFINITIONS $<TARGET_PROPERTY:${TARGET_tmp},INTERFACE_COMPILE_DEFINITIONS>)
    endforeach()
    message(STATUS "IGC targets: ${NEO__IGC_TARGETS}")
  else()
    message(FATAL_ERROR "Intel Graphics Compiler not found!")
  endif()
else()
  set(NEO__IGC_TARGETS "${IGC__IGC_TARGETS}")
  foreach(TARGET_tmp ${NEO__IGC_TARGETS})
    list(APPEND NEO__IGC_INCLUDE_DIR $<TARGET_PROPERTY:${TARGET_tmp},INTERFACE_INCLUDE_DIRECTORIES>)
    list(APPEND NEO__IGC_LIBRARY_PATH $<TARGET_FILE_DIR:${TARGET_tmp}>)
  endforeach()
  string(REPLACE ";" ":" NEO__IGC_LIBRARY_PATH "${NEO__IGC_LIBRARY_PATH}")
  message(STATUS "IGC targets: ${NEO__IGC_TARGETS}")
endif()

# VISA headers - always relative to IGC
if(IS_DIRECTORY "${IGC_DIR}/../visa")
  get_filename_component(VISA_DIR "${IGC_DIR}/../visa" ABSOLUTE)
elseif(IS_DIRECTORY "${IGC_DIR}/visa")
  set(VISA_DIR "${IGC_DIR}/visa")
elseif(IS_DIRECTORY "${IGC_DIR}/include/visa")
  set(VISA_DIR "${IGC_DIR}/include/visa")
elseif(IS_DIRECTORY "${NEO__IGC_INCLUDEDIR}/../visa")
  get_filename_component(VISA_DIR "${NEO__IGC_INCLUDEDIR}/../visa" ABSOLUTE)
elseif(IS_DIRECTORY "${IGC_OCL_ADAPTOR_DIR}/../../visa")
  get_filename_component(VISA_DIR "${IGC_OCL_ADAPTOR_DIR}/../../visa" ABSOLUTE)
endif()
message(STATUS "VISA Dir: ${VISA_DIR}")

if(IS_DIRECTORY "${VISA_DIR}/include")
  set(VISA_INCLUDE_DIR "${VISA_DIR}/include")
else()
  set(VISA_INCLUDE_DIR "${VISA_DIR}")
endif()

# IGA headers - always relative to VISA
if(IS_DIRECTORY "${VISA_DIR}/../iga")
  get_filename_component(IGA_DIR "${VISA_DIR}/../iga" ABSOLUTE)
elseif(IS_DIRECTORY "${VISA_DIR}/iga")
  set(IGA_DIR "${VISA_DIR}/iga")
endif()

if(IS_DIRECTORY "${IGA_DIR}/IGALibrary/api")
  set(IGA_INCLUDE_DIR "${IGA_DIR}/IGALibrary/api")
else()
  set(IGA_INCLUDE_DIR "${IGA_DIR}")
endif()

if(IS_DIRECTORY ${IGA_INCLUDE_DIR})
  set(IGA_HEADERS_AVAILABLE TRUE)
  set(IGA_LIBRARY_NAME "iga${NEO_BITS}")
else()
  set(IGA_HEADERS_AVAILABLE FALSE)
endif()

message(STATUS "IGA Includes dir: ${IGA_INCLUDE_DIR}")

if(WIN32)
  set(IGC_LIBRARY_NAME "igc${NEO_BITS}")
  set(FCL_LIBRARY_NAME "igdfcl${NEO_BITS}")
endif()

if(WIN32 AND NOT NEO__IGC_FOUND)
  configure_file(igc.opencl.h.in ${NEO_BUILD_DIR}/igc.opencl.h)
endif()
