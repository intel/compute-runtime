#
# Copyright (C) 2018-2021 Intel Corporation
#
# SPDX-License-Identifier: MIT
#

if(TESTS_SKL)
  set(aub_test_config "skl/1/3/8/9")
  include(${NEO_SOURCE_DIR}/cmake/run_aub_test_target.cmake)
endif()
