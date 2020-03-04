#
# Copyright (C) 2020 Intel Corporation
#
# SPDX-License-Identifier: MIT
#

if(TESTS_ICLLP)
  set(aub_test_config "icllp/1/8/8")
  include(${OPENCL_AUB_TEST_DIR}/cmake/run_aub_test_target.cmake)
endif()
