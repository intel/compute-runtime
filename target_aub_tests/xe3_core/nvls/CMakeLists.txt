#
# Copyright (C) 2026 Intel Corporation
#
# SPDX-License-Identifier: MIT
#

if(TESTS_NVLS AND "${NEO_BITS}" STREQUAL "64")
  set(aub_test_config ${NVLS_CONFIG_STR})
  include(${NEO_SOURCE_DIR}/cmake/run_aub_test_target.cmake)
endif()
