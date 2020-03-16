#
# Copyright (C) 2020 Intel Corporation
#
# SPDX-License-Identifier: MIT
#

if(TESTS_TGLLP)
  set(unit_test_config "tgllp/1/6/16")
  set(appverified_allowed ${APPVERIFIER_ALLOWED})
  include(${NEO_SOURCE_DIR}/cmake/run_ult_target.cmake)
endif()
