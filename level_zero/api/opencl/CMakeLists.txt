#
# Copyright (C) 2026 Intel Corporation
#
# SPDX-License-Identifier: MIT
#

add_subdirectory(source)

if(NOT NEO_SKIP_L0_UNIT_TESTS)
  add_subdirectory(test)
endif()
