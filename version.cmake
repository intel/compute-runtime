#
# Copyright (C) 2018-2022 Intel Corporation
#
# SPDX-License-Identifier: MIT
#

if(UNIX)
  find_program(GIT NAMES git)
  find_program(PYTHON_EXECUTABLE NAMES "python3")
  if((NOT "${GIT}" STREQUAL "GIT-NOTFOUND") AND (NOT "${PYTHON_EXECUTABLE}" STREQUAL "PYTHON_EXECUTABLE-NOTFOUND"))
    if(IS_DIRECTORY ${NEO_SOURCE_DIR}/.git)
      set(GIT_arg --git-dir=${NEO_SOURCE_DIR}/.git show -s --format=%ct)
      execute_process(
                      COMMAND ${GIT} ${GIT_arg}
                      OUTPUT_VARIABLE GIT_output
                      OUTPUT_STRIP_TRAILING_WHITESPACE
      )
      set(PYTHON_arg ${CMAKE_CURRENT_SOURCE_DIR}/scripts/neo_ww_calculator.py ${GIT_output})
      execute_process(
                      COMMAND ${PYTHON_EXECUTABLE} ${PYTHON_arg}
                      OUTPUT_VARIABLE VERSION_output
                      OUTPUT_STRIP_TRAILING_WHITESPACE
      )
      string(REPLACE "." ";" VERSION_list ${VERSION_output})
    endif()
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

if(NOT DEFINED NEO_VERSION_BUILD)
  set(NEO_VERSION_BUILD 0)
  set(NEO_REVISION 0)
else()
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
endif()
if(NOT DEFINED NEO_REVISION)
  set(NEO_REVISION 0)
endif()

# OpenCL pacakge version
set(NEO_OCL_DRIVER_VERSION "${NEO_OCL_VERSION_MAJOR}.${NEO_OCL_VERSION_MINOR}.${NEO_VERSION_BUILD}")

# Level-Zero package version
set(NEO_L0_VERSION_MAJOR 1)
set(NEO_L0_VERSION_MINOR 3)
