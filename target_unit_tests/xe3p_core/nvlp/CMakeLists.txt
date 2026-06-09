#
# Copyright (C) 2026 Intel Corporation
#
# SPDX-License-Identifier: MIT
#

if(TESTS_NVLP)
  set(unit_test_config "nvlp/2/4/8/4")
  include(${NEO_SOURCE_DIR}/cmake/run_ult_target.cmake)
endif()
