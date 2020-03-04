#
# Copyright (C) 2020 Intel Corporation
#
# SPDX-License-Identifier: MIT
#

if(TESTS_SKL)
  set(mt_test_config "skl/1/3/8")
  include(${OPENCL_MT_TEST_DIR}/cmake/run_mt_test_target.cmake)
endif()
