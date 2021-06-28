#
# Copyright (C) 2020-2021 Intel Corporation
#
# SPDX-License-Identifier: MIT
#

if(TESTS_TGLLP)
  set(aub_test_config "tgllp/1/6/16/0")
  include(${NEO_SOURCE_DIR}/cmake/run_aub_test_target.cmake)
endif()
