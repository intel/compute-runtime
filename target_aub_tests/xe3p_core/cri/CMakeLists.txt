#
# Copyright (C) 2026 Intel Corporation
#
# SPDX-License-Identifier: MIT
#

if(TESTS_CRI AND "${NEO_BITS}" STREQUAL "64")
  set(aub_test_config ${CRI_CONFIG_STR})
  set(aub_test_config_blitter_mask "6")
  include(${NEO_SOURCE_DIR}/cmake/run_aub_test_target.cmake)
endif()
