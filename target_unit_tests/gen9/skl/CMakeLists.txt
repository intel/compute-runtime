#
# Copyright (C) 2020 Intel Corporation
#
# SPDX-License-Identifier: MIT
#

if(TESTS_SKL)
  set(unit_test_config  "skl/1/3/8")
  set(appverified_allowed ${APPVERIFIER_ALLOWED})
  include(${NEO_SOURCE_DIR}/cmake/run_ult_target.cmake)
endif()
