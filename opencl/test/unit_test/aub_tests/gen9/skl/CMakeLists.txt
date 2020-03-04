#
# Copyright (C) 2017-2020 Intel Corporation
#
# SPDX-License-Identifier: MIT
#

if(TESTS_SKL)
  add_subdirectories()
  set(aub_test_config "skl/1/3/8")
  include(${OPENCL_AUB_TEST_DIR}/cmake/run_aub_test_target.cmake)
endif()
