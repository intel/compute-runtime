#
# Copyright (C) 2021-2022 Intel Corporation
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
                 ${CMAKE_CURRENT_SOURCE_DIR}/hw_helper_xe_hp_core_tests.cpp
                 ${CMAKE_CURRENT_SOURCE_DIR}/image_surface_state_tests_xe_hp_core.cpp
                 ${CMAKE_CURRENT_SOURCE_DIR}/product_config_tests_xe_hp_core.cpp
                 ${CMAKE_CURRENT_SOURCE_DIR}/simd_helper_tests_xe_hp_core.cpp
                 ${CMAKE_CURRENT_SOURCE_DIR}/test_encode_xe_hp_core.cpp
                 ${CMAKE_CURRENT_SOURCE_DIR}/test_hw_info_config_xe_hp_core.cpp
                 ${CMAKE_CURRENT_SOURCE_DIR}/test_preemption_xe_hp_core.cpp
  )

  if(DEFINED AUB_STREAM_PROJECT_NAME)
    target_sources(neo_shared_tests PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/aub_center_using_aubstream_lib_tests_xe_hp_core.cpp)
  endif()

  add_subdirectories()
endif()
