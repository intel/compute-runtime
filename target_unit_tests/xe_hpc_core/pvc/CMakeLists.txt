#
# Copyright (C) 2021-2024 Intel Corporation
#
# SPDX-License-Identifier: MIT
#

if(TESTS_PVC)
  set(unit_test_config "pvc/2/4/8/47") # non-zero values for unit tests
  include(${NEO_SOURCE_DIR}/cmake/run_ult_target.cmake)
endif()
