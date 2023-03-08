#
# Copyright (C) 2021-2023 Intel Corporation
#
# SPDX-License-Identifier: MIT
#

if(TESTS_XE_HP_CORE)
  target_sources(neo_shared_tests PRIVATE
                 ${CMAKE_CURRENT_SOURCE_DIR}/CMakeLists.txt
                 ${CMAKE_CURRENT_SOURCE_DIR}/command_encoder_tests_xe_hp_core.cpp
                 ${CMAKE_CURRENT_SOURCE_DIR}/compute_mode_tests_xe_hp_core.cpp
                 ${CMAKE_CURRENT_SOURCE_DIR}/excludes_xe_hp_core.cpp
                 ${CMAKE_CURRENT_SOURCE_DIR}/hw_cmds_xe_hp_core_tests.cpp
                 ${CMAKE_CURRENT_SOURCE_DIR}/gfx_core_helper_xe_hp_core_tests.cpp
                 ${CMAKE_CURRENT_SOURCE_DIR}/image_surface_state_tests_xe_hp_core.cpp
                 ${CMAKE_CURRENT_SOURCE_DIR}/product_config_tests_xe_hp_core.cpp
                 ${CMAKE_CURRENT_SOURCE_DIR}/simd_helper_tests_xe_hp_core.cpp
                 ${CMAKE_CURRENT_SOURCE_DIR}/test_encode_xe_hp_core.cpp
                 ${CMAKE_CURRENT_SOURCE_DIR}/test_product_helper_xe_hp_core.cpp
                 ${CMAKE_CURRENT_SOURCE_DIR}/test_preemption_xe_hp_core.cpp
  )

  add_subdirectories()
endif()
