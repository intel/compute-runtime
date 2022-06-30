#
# Copyright (C) 2020-2021 Intel Corporation
#
# SPDX-License-Identifier: MIT
#

if(NOT NEO_SKIP_L0_UNIT_TESTS)
  add_subdirectory(unit_tests)
endif()

if(NOT NEO_SKIP_L0_BLACK_BOX_TESTS)
  add_subdirectory(black_box_tests)
endif()
