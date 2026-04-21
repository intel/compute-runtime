#
# Copyright (C) 2025-2026 Intel Corporation
#
# SPDX-License-Identifier: MIT
#

if(TESTS_MTL)
  set(aub_test_config ${MTL_CONFIG_STR})
  include(${NEO_SOURCE_DIR}/cmake/run_aub_test_target.cmake)
endif()
