#
# Copyright (C) 2021 Intel Corporation
#
# SPDX-License-Identifier: MIT
#

if(TESTS_XE_HP_SDV)
  set(unit_test_config "xe_hp_sdv/2/4/5/4") # non-zero values for unit tests
  include(${NEO_SOURCE_DIR}/cmake/run_ult_target.cmake)
endif()
