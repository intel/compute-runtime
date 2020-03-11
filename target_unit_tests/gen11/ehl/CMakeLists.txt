#
# Copyright (C) 2020 Intel Corporation
#
# SPDX-License-Identifier: MIT
#

if(TESTS_EHL)
  set(unit_test_config "ehl/1/4/8")
  include(${NEO_SOURCE_DIR}/cmake/run_ult_target.cmake)
endif()
