#
# Copyright (C) 2024 Intel Corporation
#
# SPDX-License-Identifier: MIT
#

if(TESTS_XE2_HPG_CORE)
  set(NEO_CORE_tests_xe2_hpg_core_excludes ${CMAKE_CURRENT_SOURCE_DIR}/excludes_xe2_hpg_core.cpp)
  set_property(GLOBAL APPEND PROPERTY NEO_CORE_tests_excludes ${NEO_CORE_tests_xe2_hpg_core_excludes})

  target_sources(neo_shared_tests PRIVATE
                 ${NEO_CORE_tests_xe2_hpg_core_excludes}
                 ${CMAKE_CURRENT_SOURCE_DIR}/CMakeLists.txt
                 ${CMAKE_CURRENT_SOURCE_DIR}/aub_command_stream_receiver_2_tests_xe2_hpg_core.cpp
                 ${CMAKE_CURRENT_SOURCE_DIR}/compute_mode_tests_xe2_hpg_core.cpp
                 ${CMAKE_CURRENT_SOURCE_DIR}/gfx_core_helper_tests_xe2_hpg_core.cpp
                 ${CMAKE_CURRENT_SOURCE_DIR}/hw_cmds_xe2_hpg_core_tests.cpp
                 ${CMAKE_CURRENT_SOURCE_DIR}/image_surface_state_tests_xe2_hpg_core.cpp
                 ${CMAKE_CURRENT_SOURCE_DIR}/simd_helper_tests_xe2_hpg_core.cpp
                 ${CMAKE_CURRENT_SOURCE_DIR}/test_blit_commands_helper_xe2_hpg_core.cpp
                 ${CMAKE_CURRENT_SOURCE_DIR}/test_device_caps_xe2_hpg_core.cpp
                 ${CMAKE_CURRENT_SOURCE_DIR}/test_encode_3dstate_btd_xe2_hpg_core.cpp
                 ${CMAKE_CURRENT_SOURCE_DIR}/test_encode_xe2_hpg_core.cpp
                 ${CMAKE_CURRENT_SOURCE_DIR}/test_preemption_xe2_hpg_core.cpp
  )

  if(DEFINED AUB_STREAM_PROJECT_NAME)
    target_sources(neo_shared_tests PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/aub_center_using_aubstream_lib_tests_xe2_hpg_core.cpp)
  endif()

  add_subdirectories()
endif()
