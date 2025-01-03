#
# Copyright (C) 2025 Intel Corporation
#
# SPDX-License-Identifier: MIT
#

if(TESTS_XE3_CORE)
  set(NEO_CORE_tests_xe3_core_excludes ${CMAKE_CURRENT_SOURCE_DIR}/excludes_xe3_core.cpp)
  set_property(GLOBAL APPEND PROPERTY NEO_CORE_tests_excludes ${NEO_CORE_tests_xe3_core_excludes})

  target_sources(neo_shared_tests PRIVATE
                 ${NEO_CORE_tests_xe3_core_excludes}
                 ${CMAKE_CURRENT_SOURCE_DIR}/CMakeLists.txt
                 ${CMAKE_CURRENT_SOURCE_DIR}/aub_command_stream_receiver_2_tests_xe3_core.cpp
                 ${CMAKE_CURRENT_SOURCE_DIR}/command_encoder_xe3_core_tests.cpp
                 ${CMAKE_CURRENT_SOURCE_DIR}/compute_mode_tests_xe3_core.cpp
                 ${CMAKE_CURRENT_SOURCE_DIR}/engine_node_helper_tests_xe3_core.cpp
                 ${CMAKE_CURRENT_SOURCE_DIR}/gfx_core_helper_xe3_core_tests.cpp
                 ${CMAKE_CURRENT_SOURCE_DIR}/hw_cmds_xe3_core_tests.cpp
                 ${CMAKE_CURRENT_SOURCE_DIR}/hw_info_tests_xe3_core.cpp
                 ${CMAKE_CURRENT_SOURCE_DIR}/image_surface_state_tests_xe3_core.cpp
                 ${CMAKE_CURRENT_SOURCE_DIR}/product_helper_tests_xe3_core.cpp
                 ${CMAKE_CURRENT_SOURCE_DIR}/simd_helper_tests_xe3_core.cpp
                 ${CMAKE_CURRENT_SOURCE_DIR}/test_device_caps_xe3_core.cpp
                 ${CMAKE_CURRENT_SOURCE_DIR}/test_encode_3dstate_btd_xe3_core.cpp
                 ${CMAKE_CURRENT_SOURCE_DIR}/test_encode_xe3_core.cpp
                 ${CMAKE_CURRENT_SOURCE_DIR}/test_preemption_xe3_core.cpp
  )

  add_subdirectories()
endif()
