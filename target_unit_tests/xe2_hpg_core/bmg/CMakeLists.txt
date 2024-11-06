#
# Copyright (C) 2024 Intel Corporation
#
# SPDX-License-Identifier: MIT
#

if(TESTS_BMG)
  set(unit_test_config "bmg/2/4/8/0")
  include(${NEO_SOURCE_DIR}/cmake/run_ult_target.cmake)
endif()
