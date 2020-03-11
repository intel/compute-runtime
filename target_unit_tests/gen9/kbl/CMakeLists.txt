#
# Copyright (C) 2020 Intel Corporation
#
# SPDX-License-Identifier: MIT
#

if(TESTS_KBL)
  set(unit_test_config "kbl/1/3/6")
  include(${NEO_SOURCE_DIR}/cmake/run_ult_target.cmake)
endif()
