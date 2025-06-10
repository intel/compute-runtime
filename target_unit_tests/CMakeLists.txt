#
# Copyright (C) 2020-2025 Intel Corporation
#
# SPDX-License-Identifier: MIT
#

set(BUILT_IN_KERNEL_DIR "${NEO_SOURCE_DIR}/shared/source/built_ins")
set(PLATFORM_SPECIFIC_TEST_TARGETS_FOLDER ult_product_targets)

if(NOT TARGET run_mt_unit_tests AND NOT NEO_SKIP_MT_UNIT_TESTS)
  add_custom_target(run_mt_unit_tests)
endif()

if(NOT TARGET run_unit_tests)
  add_custom_target(run_unit_tests ALL)
  set_target_properties(run_unit_tests PROPERTIES FOLDER ${PLATFORM_SPECIFIC_TEST_TARGETS_FOLDER})
endif()

if(NOT TARGET unit_tests)
  add_custom_target(unit_tests ALL)
  set_target_properties(unit_tests PROPERTIES FOLDER ${PLATFORM_SPECIFIC_TEST_TARGETS_FOLDER})
endif()

add_subdirectories()
