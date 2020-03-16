#
# Copyright (C) 2020 Intel Corporation
#
# SPDX-License-Identifier: MIT
#

if(TESTS_ICLLP)
  set(unit_test_config "icllp/1/8/8")
  set(appverified_allowed ${APPVERIFIER_ALLOWED})
  include(${NEO_SOURCE_DIR}/cmake/run_ult_target.cmake)
endif()
