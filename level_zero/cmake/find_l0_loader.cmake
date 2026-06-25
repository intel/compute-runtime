#
# Copyright (C) 2026 Intel Corporation
#
# SPDX-License-Identifier: MIT
#

if("${L0_BLACK_BOX_TEST_USE_DRIVER}" STREQUAL "1")
  message(STATUS "Using driver as a loader to link black box tests")
  set(L0_LOADER_LIBRARY "${TARGET_NAME_L0}")
elseif(NOT "${EXTERNAL_L0_LOADER_PATH}" STREQUAL "")
  if(NOT EXISTS "${EXTERNAL_L0_LOADER_PATH}")
    message(FATAL_ERROR "EXTERNAL_L0_LOADER_PATH does not exist: ${EXTERNAL_L0_LOADER_PATH}")
  endif()
  set(L0_LOADER_LIBRARY "${EXTERNAL_L0_LOADER_PATH}")
else()
  find_package(PkgConfig QUIET)
  if(PkgConfig_FOUND)
    pkg_check_modules(NEO__L0_LOADER QUIET libze_loader)
  endif()
  find_library(L0_LOADER_LIBRARY NAMES ze_loader
               HINTS "${LEVEL_ZERO_ROOT}/lib"
               "${LEVEL_ZERO_ROOT}/lib64"
               "${LEVEL_ZERO_ROOT}/bin"
               "${NEO__L0_LOADER_LIBRARY_DIRS}"
  )
endif()

if(L0_LOADER_LIBRARY)
  set(NEO__L0_LOADER_FOUND TRUE)
else()
  set(NEO__L0_LOADER_FOUND FALSE)
endif()
