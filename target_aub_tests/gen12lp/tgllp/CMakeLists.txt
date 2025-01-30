#
# Copyright (C) 2020-2025 Intel Corporation
#
# SPDX-License-Identifier: MIT
#

if(TESTS_TGLLP)
  set(aub_test_config ${TGLLP_CONFIG_STR})
  include(${NEO_SOURCE_DIR}/cmake/run_aub_test_target.cmake)
endif()
