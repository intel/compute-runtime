#
# Copyright (C) 2021-2022 Intel Corporation
#
# SPDX-License-Identifier: MIT
#

# DEPENDENCY DETECTION
function(dependency_detect COMPONENT_NAME DLL_NAME VAR_NAME REL_LOCATION IS_THIRD_PARTY)
  if(DEFINED ${VAR_NAME}_DIR)
    get_filename_component(LIBRARY_DIR "${${VAR_NAME}_DIR}" ABSOLUTE)
  else()
    get_filename_component(LIBRARY_DIR_tmp "${NEO_SOURCE_DIR}/${REL_LOCATION}" ABSOLUTE)
    if(IS_DIRECTORY "${LIBRARY_DIR_tmp}")
      set(LIBRARY_DIR "${LIBRARY_DIR_tmp}")
    endif()
  endif()
  if(UNIX)
    if(DEFINED LIBRARY_DIR)
      if(IS_DIRECTORY "${LIBRARY_DIR}/lib/pkgconfig/")
        set(__tmp_LIBDIR "lib")
      elseif(IS_DIRECTORY "${LIBRARY_DIR}/${CMAKE_INSTALL_LIBDIR}/pkgconfig/")
        set(__tmp_LIBDIR ${CMAKE_INSTALL_LIBDIR})
      endif()
    endif()

    find_package(PkgConfig)
    if(DEFINED __tmp_LIBDIR)
      set(OLD_PKG_CONFIG_PATH $ENV{PKG_CONFIG_PATH})
      set(ENV{PKG_CONFIG_PATH} "${LIBRARY_DIR}/${__tmp_LIBDIR}/pkgconfig/")
    endif()
    if(NOT DLL_NAME STREQUAL "")
      pkg_check_modules(NEO__${VAR_NAME} ${DLL_NAME})
    endif()
    if(DEFINED __tmp_LIBDIR)
      set(ENV{PKG_CONFIG_PATH} ${OLD_PKG_CONFIG_PATH})
    endif()

    if(NEO__${VAR_NAME}_FOUND)
      if(DEFINED __tmp_LIBDIR)
        if(NOT NEO__${VAR_NAME}_INCLUDE_DIRS STREQUAL "")
          string(REPLACE "${NEO__${VAR_NAME}_INCLUDEDIR}" "${LIBRARY_DIR}/include" NEO__${VAR_NAME}_INCLUDE_DIRS "${NEO__${VAR_NAME}_INCLUDE_DIRS}")
        else()
          set(NEO__${VAR_NAME}_INCLUDE_DIRS "${LIBRARY_DIR}/include")
        endif()
      endif()

      set(NEO__${VAR_NAME}_INCLUDE_DIR "${NEO__${VAR_NAME}_INCLUDE_DIRS}")
      set(NEO__${VAR_NAME}_INCLUDE_DIR "${NEO__${VAR_NAME}_INCLUDE_DIRS}" PARENT_SCOPE)

      set(NEO__${VAR_NAME}_LIBRARIES ${NEO__${VAR_NAME}_LIBRARIES} PARENT_SCOPE)

      message(STATUS "${COMPONENT_NAME} include dirs: ${NEO__${VAR_NAME}_INCLUDE_DIR}")

      return()
    endif()
  else()
    # Windows
  endif()

  if(IS_THIRD_PARTY)
    string(TOLOWER ${VAR_NAME} _VAR_NAME_LOWER)
    get_filename_component(${VAR_NAME}_HEADERS_DIR "${CMAKE_CURRENT_SOURCE_DIR}/third_party${BRANCH_DIR_SUFFIX}/${_VAR_NAME_LOWER}" ABSOLUTE)
    if(IS_DIRECTORY ${${VAR_NAME}_HEADERS_DIR})
      message(STATUS "${COMPONENT_NAME} dir: ${${VAR_NAME}_HEADERS_DIR}")
      set(NEO__${VAR_NAME}_INCLUDE_DIR "${${VAR_NAME}_HEADERS_DIR}" PARENT_SCOPE)
      return()
    endif()
  endif()

  message(FATAL_ERROR "${COMPONENT_NAME} not found!")

endfunction()

# Metrics Library Detection
dependency_detect("Metrics Library" libigdml METRICS_LIBRARY "../metrics/library" TRUE)
if(NOT NEO__METRICS_LIBRARY_INCLUDE_DIR STREQUAL "")
  include_directories("${NEO__METRICS_LIBRARY_INCLUDE_DIR}")
endif()

# Metrics Discovery Detection
dependency_detect("Metrics Discovery" "" METRICS_DISCOVERY "../metrics/discovery" TRUE)
if(NOT NEO__METRICS_DISCOVERY_INCLUDE_DIR STREQUAL "")
  include_directories("${NEO__METRICS_DISCOVERY_INCLUDE_DIR}")
endif()

