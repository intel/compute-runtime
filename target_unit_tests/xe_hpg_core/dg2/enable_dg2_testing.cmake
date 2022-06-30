#
# Copyright (C) 2021 Intel Corporation
#
# SPDX-License-Identifier: MIT
#

if(TESTS_DG2)
  set(unit_test_config "dg2/2/4/5/0") # non-zero values for unit tests
  include(${NEO_SOURCE_DIR}/cmake/run_ult_target.cmake)
endif()
