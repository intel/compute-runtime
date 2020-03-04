#
# Copyright (C) 2020 Intel Corporation
#
# SPDX-License-Identifier: MIT
#

if(TESTS_BXT)
  set(aub_test_config "bxt/1/3/6")
  include(${OPENCL_AUB_TEST_DIR}/cmake/run_aub_test_target.cmake)
endif()
