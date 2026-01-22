#
# Copyright (C) 2018-2026 Intel Corporation
#
# SPDX-License-Identifier: MIT
#

include(${CMAKE_CURRENT_SOURCE_DIR}/scripts/neo_ww_calculator.cmake)

if(UNIX)
  find_program(GIT NAMES git)
  if(NOT "${GIT}" STREQUAL "GIT-NOTFOUND" AND IS_DIRECTORY ${NEO_SOURCE_DIR}/.git)
    set(GIT_arg --git-dir=${NEO_SOURCE_DIR}/.git show -s --format=%ct)
    execute_process(
                    COMMAND ${GIT} ${GIT_arg}
                    OUTPUT_VARIABLE GIT_output
                    OUTPUT_STRIP_TRAILING_WHITESPACE
    )
    neo_ww_calculator(${GIT_output} VERSION_output)
    string(REPLACE "." ";" VERSION_list ${VERSION_output})
  else()
    message(WARNING "Unable to determine OpenCL major.minor version. Defaulting to 1.0")
  endif()

  if(NOT DEFINED NEO_OCL_VERSION_MAJOR)
    if(NOT DEFINED VERSION_list)
      set(NEO_OCL_VERSION_MAJOR 1)
    else()
      list(GET VERSION_list 0 NEO_OCL_VERSION_MAJOR)
      message(STATUS "Computed OpenCL version major is: ${NEO_OCL_VERSION_MAJOR}")
    endif()
  endif()

  if(NOT DEFINED NEO_OCL_VERSION_MINOR)
    if(NOT DEFINED VERSION_list)
      set(NEO_OCL_VERSION_MINOR 0)
    else()
      list(GET VERSION_list 1 NEO_OCL_VERSION_MINOR)
      message(STATUS "Computed OpenCL version minor is: ${NEO_OCL_VERSION_MINOR}")
    endif()
  endif()
else()
  if(NOT DEFINED NEO_OCL_VERSION_MAJOR)
    set(NEO_OCL_VERSION_MAJOR 1)
  endif()

  if(NOT DEFINED NEO_OCL_VERSION_MINOR)
    set(NEO_OCL_VERSION_MINOR 0)
  endif()
endif()

find_program(GIT NAMES git)
if(NOT "${GIT}" STREQUAL "GIT-NOTFOUND")
  if(IS_DIRECTORY ${NEO_SOURCE_DIR}/.git)
    set(GIT_arg --git-dir=${NEO_SOURCE_DIR}/.git rev-parse HEAD)
    execute_process(
                    COMMAND ${GIT} ${GIT_arg}
                    OUTPUT_VARIABLE NEO_REVISION
                    OUTPUT_STRIP_TRAILING_WHITESPACE
    )
  endif()
endif()

if(NOT DEFINED NEO_REVISION)
  set(NEO_REVISION "No git SHA found, compiled outside git folder")
endif()

if(NOT DEFINED NEO_VERSION_BUILD)
  set(NEO_VERSION_BUILD 0)
endif()

if(NOT DEFINED NEO_VERSION_HOTFIX)
  set(NEO_VERSION_HOTFIX 0)
endif()

set(NEO_OCL_VERSION_SUFFIX "")
if(NOT ("${NEO_VERSION_HOTFIX}" STREQUAL "0"))
  set(NEO_OCL_VERSION_SUFFIX ".${NEO_VERSION_HOTFIX}")
endif()

if(NEO_VERSION_BUILD MATCHES "^([0-9]+)\\.([0-9]+)$")
  set(NEO_VERSION_BUILD "${CMAKE_MATCH_1}")
  if(NOT ("${NEO_OCL_VERSION_SUFFIX}" STREQUAL "") AND NOT ("${NEO_OCL_VERSION_SUFFIX}" STREQUAL ".${CMAKE_MATCH_2}"))
    message(FATAL_ERROR "Inconsistent hotfix version provided: ${NEO_VERSION_HOTFIX} vs ${CMAKE_MATCH_2}")
  endif()
  set(NEO_VERSION_HOTFIX "${CMAKE_MATCH_2}")
  set(NEO_OCL_VERSION_SUFFIX ".${NEO_VERSION_HOTFIX}")
endif()

# OpenCL package version
set(NEO_OCL_DRIVER_VERSION "${NEO_OCL_VERSION_MAJOR}.${NEO_OCL_VERSION_MINOR}.${NEO_VERSION_BUILD}${NEO_OCL_VERSION_SUFFIX}")

# Level-Zero package version
set(NEO_L0_VERSION_MAJOR 1)
set(NEO_L0_VERSION_MINOR 14)

# Remove leading zeros
string(REGEX REPLACE "^0+([0-9]+)" "\\1" NEO_VERSION_BUILD "${NEO_VERSION_BUILD}")
string(REGEX REPLACE "^0+([0-9]+)" "\\1" NEO_VERSION_HOTFIX "${NEO_VERSION_HOTFIX}")
