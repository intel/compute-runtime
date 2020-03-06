#
# Copyright (C) 2020 Intel Corporation
#
# SPDX-License-Identifier: MIT
#

include(FindPackageHandleStandardArgs)

find_path(LevelZero_INCLUDE_DIR
  NAMES level_zero/ze_api.h
  PATHS
    ${LEVEL_ZERO_ROOT}
  PATH_SUFFIXES "include"
)

find_package_handle_standard_args(LevelZero
  REQUIRED_VARS
    LevelZero_INCLUDE_DIR
)

if(LevelZero_FOUND)
    list(APPEND LevelZero_INCLUDE_DIRS ${LevelZero_INCLUDE_DIR})
endif()

if(LevelZero_FOUND AND NOT TARGET LevelZero::LevelZero)
    add_library(LevelZero::LevelZero INTERFACE IMPORTED)
    set_target_properties(LevelZero::LevelZero
      PROPERTIES INTERFACE_INCLUDE_DIRECTORIES "${LevelZero_INCLUDE_DIRS}"
    )
endif()

MESSAGE(STATUS "LevelZero_INCLUDE_DIRS: " ${LevelZero_INCLUDE_DIRS})
