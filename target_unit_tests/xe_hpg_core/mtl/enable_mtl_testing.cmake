#
# Copyright (C) 2022 Intel Corporation
#
# SPDX-License-Identifier: MIT
#

if(TESTS_MTL)
  set(unit_test_config "mtl/2/4/5/0") # non-zero values for unit tests
  include(${NEO_SOURCE_DIR}/cmake/run_ult_target.cmake)
endif()
