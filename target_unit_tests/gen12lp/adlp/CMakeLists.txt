#
# Copyright (C) 2021-2026 Intel Corporation
#
# SPDX-License-Identifier: MIT
#

if(TESTS_ADLP)
  set(unit_test_config "adlp/1/2/8/0")
  include(${NEO_SOURCE_DIR}/cmake/run_ult_target.cmake)
endif()
