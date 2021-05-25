#
# Copyright (C) 2018-2021 Intel Corporation
#
# SPDX-License-Identifier: MIT
#

if(TESTS_GEN9)
  target_sources(igdrcl_aub_tests PRIVATE
                 ${NEO_SOURCE_DIR}/shared/test/common/gen9/unit_test_helper_gen9.cpp
  )
  add_subdirectories()
endif()
