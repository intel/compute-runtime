#
# Copyright (C) 2025 Intel Corporation
#
# SPDX-License-Identifier: MIT
#

if(TESTS_PTL)
  set(unit_test_config "ptl/2/4/8/4")
  include(${NEO_SOURCE_DIR}/cmake/run_ult_target.cmake)
endif()
