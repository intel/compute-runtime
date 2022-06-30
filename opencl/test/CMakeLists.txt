#
# Copyright (C) 2021 Intel Corporation
#
# SPDX-License-Identifier: MIT
#

if(DEFAULT_TESTED_PLATFORM AND NOT NEO_SKIP_OCL_UNIT_TESTS)
  add_subdirectory_unique(unit_test ${NEO_BUILD_DIR}/opencl/test/unit_test)
endif()

if(NOT BUILD_WITHOUT_RUNTIME)
  add_subdirectory_unique(black_box_test)
endif()

