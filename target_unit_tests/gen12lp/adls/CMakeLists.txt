#
# Copyright (C) 2020 Intel Corporation
#
# SPDX-License-Identifier: MIT
#

if(TESTS_ADLS)
  set(unit_test_config "adls/1/2/16/0")
  include(${NEO_SOURCE_DIR}/cmake/run_ult_target.cmake)
endif()
