#
# Copyright (C) 2020-2025 Intel Corporation
#
# SPDX-License-Identifier: MIT
#

set(AUBCONFIG_PATH "${NEO_SOURCE_DIR}/../aub_configs/CMakeLists.txt")

if(DEFINED AUB_STREAM_PROJECT_NAME AND EXISTS ${AUBCONFIG_PATH})
  add_custom_target(run_aub_tests)
  set(AUB_TESTS_TARGETS_FOLDER "aub_product_targets")
  set_target_properties(run_aub_tests PROPERTIES FOLDER ${AUB_TESTS_TARGETS_FOLDER})
  set_property(GLOBAL PROPERTY NEO_PLATFORMS_FOR_AUB_GENERATION)
  include(${AUBCONFIG_PATH})

  add_subdirectories()

  if(DEFINED NEO_GENERATE_AUBS_FOR)
    get_property(NEO_PLATFORMS_FOR_AUB_GENERATION GLOBAL PROPERTY NEO_PLATFORMS_FOR_AUB_GENERATION)
    message(STATUS "Aub generation scope restricted by NEO_GENERATE_AUBS_FOR flag")
    message(STATUS "All tested platforms with aub generation: " ${NEO_PLATFORMS_FOR_AUB_GENERATION})
  endif()
endif()
