#
# Copyright (C) 2021 Intel Corporation
#
# SPDX-License-Identifier: MIT
#

if(TESTS_ADLP)
  set(unit_test_config "adlp/1/2/8/0") # non-zero values for unit tests
  include(${NEO_SOURCE_DIR}/cmake/run_ult_target.cmake)
endif()
