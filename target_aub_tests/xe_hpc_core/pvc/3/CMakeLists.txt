#
# Copyright (C) 2025 Intel Corporation
#
# SPDX-License-Identifier: MIT
#

if(TESTS_PVC AND "${NEO_BITS}" STREQUAL "64")
  set(aub_test_config ${PVC_CONFIG_STR})
  include(${NEO_SOURCE_DIR}/cmake/run_aub_test_target.cmake)
endif()
