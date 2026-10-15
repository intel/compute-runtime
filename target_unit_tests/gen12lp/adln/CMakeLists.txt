#
# Copyright (C) 2022-2026 Intel Corporation
#
# SPDX-License-Identifier: MIT
#

if(TESTS_ADLN)
  set(unit_test_config "adln/1/2/8/0")
  include(${NEO_SOURCE_DIR}/cmake/run_ult_target.cmake)
endif()
