#
# Copyright (C) 2020 Intel Corporation
#
# SPDX-License-Identifier: MIT
#

if(TESTS_TGLLP)
  set(mt_test_config "tgllp/1/6/16")
  include(${OPENCL_MT_TEST_DIR}/cmake/run_mt_test_target.cmake)
endif()
