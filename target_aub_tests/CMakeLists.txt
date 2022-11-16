#
# Copyright (C) 2020-2022 Intel Corporation
#
# SPDX-License-Identifier: MIT
#

if(DEFINED AUB_STREAM_PROJECT_NAME)
  add_custom_target(run_aub_tests)
  set(AUB_TESTS_TARGETS_FOLDER "aub_product_targets")
  set_target_properties(run_aub_tests PROPERTIES FOLDER ${AUB_TESTS_TARGETS_FOLDER})

  add_subdirectories()
endif()
