#
# Copyright (C) 2022 Intel Corporation
#
# SPDX-License-Identifier: MIT
#

if(TESTS_ADLN)
  set(unit_test_config "adln/1/2/8/0") # non-zero values for unit tests
  include(${NEO_SOURCE_DIR}/cmake/run_ult_target.cmake)
endif()
