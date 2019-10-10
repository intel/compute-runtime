#
# Copyright (C) 2018-2019 Intel Corporation
#
# SPDX-License-Identifier: MIT
#

if(UNIX)
  if(NOT DEFINED NEO_DRIVER_VERSION)
    find_program(GIT NAMES git)
    if(NOT "${GIT}" STREQUAL "GIT-NOTFOUND")
      if(IS_DIRECTORY ${NEO_SOURCE_DIR}/.git)
        set(GIT_arg --git-dir=${NEO_SOURCE_DIR}/.git show -s --format=%ct)
        execute_process(
          COMMAND ${GIT} ${GIT_arg}
          OUTPUT_VARIABLE GIT_output
          OUTPUT_STRIP_TRAILING_WHITESPACE
        )
      endif()
    endif()

    if(NOT DEFINED NEO_VERSION_MAJOR)
      if(NOT DEFINED GIT_output)
        set(NEO_VERSION_MAJOR 1)
      else()
        SET(DATE_arg --date=@${GIT_output} +%y)
        execute_process(
          COMMAND date ${DATE_arg}
          OUTPUT_VARIABLE NEO_VERSION_MAJOR
          OUTPUT_STRIP_TRAILING_WHITESPACE
        )
        message(STATUS "Computed version major is: ${NEO_VERSION_MAJOR}")
      endif()
    endif()

    if(NOT DEFINED NEO_VERSION_MINOR)
      if(NOT DEFINED GIT_output)
        set(NEO_VERSION_MINOR 0)
      else()
        SET(DATE_arg --date=@${GIT_output} +%V)
        execute_process(
          COMMAND date ${DATE_arg}
          OUTPUT_VARIABLE NEO_VERSION_MINOR
          OUTPUT_STRIP_TRAILING_WHITESPACE
        )
        message(STATUS "Computed version minor is: ${NEO_VERSION_MINOR}")
      endif()
    endif()

    if(NOT DEFINED NEO_VERSION_BUILD)
      set(NEO_VERSION_BUILD 0)
    endif()
    set(NEO_DRIVER_VERSION "${NEO_VERSION_MAJOR}.${NEO_VERSION_MINOR}.${NEO_VERSION_BUILD}")
  endif()
else()
  if(NOT DEFINED NEO_VERSION_MAJOR)
    set(NEO_VERSION_MAJOR 1)
  endif()

  if(NOT DEFINED NEO_VERSION_MINOR)
    set(NEO_VERSION_MINOR 0)
  endif()

  if(NOT DEFINED NEO_VERSION_BUILD)
    set(NEO_VERSION_BUILD 0)
  endif()
  set(NEO_DRIVER_VERSION "${NEO_VERSION_MAJOR}.${NEO_VERSION_MINOR}.${NEO_VERSION_BUILD}")
endif(UNIX)
