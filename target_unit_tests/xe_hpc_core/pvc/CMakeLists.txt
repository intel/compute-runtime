#
# Copyright (C) 2021-2025 Intel Corporation
#
# SPDX-License-Identifier: MIT
#

if(TESTS_PVC)
  if(MSVC AND NOT "${NEO_BITS}" STREQUAL "64")
    return()
  endif()

  set(unit_test_config "pvc/2/4/8/47") # non-zero values for unit tests
  include(${NEO_SOURCE_DIR}/cmake/run_ult_target.cmake)
endif()
