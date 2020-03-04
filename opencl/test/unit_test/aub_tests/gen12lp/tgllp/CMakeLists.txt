#
# Copyright (C) 2020 Intel Corporation
#
# SPDX-License-Identifier: MIT
#

if(TESTS_TGLLP)
  set(aub_test_config "tgllp/1/6/16")
  include(${OPENCL_AUB_TEST_DIR}/cmake/run_aub_test_target.cmake)
endif()
