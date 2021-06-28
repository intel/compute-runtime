#
# Copyright (C) 2020-2021 Intel Corporation
#
# SPDX-License-Identifier: MIT
#

if(TESTS_BXT)
  set(aub_test_config "bxt/1/3/6/0")
  include(${NEO_SOURCE_DIR}/cmake/run_aub_test_target.cmake)
endif()
