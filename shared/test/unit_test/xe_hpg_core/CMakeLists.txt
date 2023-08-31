#
# Copyright (C) 2021-2023 Intel Corporation
#
# SPDX-License-Identifier: MIT
#

if(TESTS_XE_HPG_CORE)
  set(NEO_CORE_tests_xe_hpg_core_excludes ${CMAKE_CURRENT_SOURCE_DIR}/excludes_xe_hpg_core.cpp)
  set_property(GLOBAL APPEND PROPERTY NEO_CORE_tests_excludes ${NEO_CORE_tests_xe_hpg_core_excludes})

  target_sources(neo_shared_tests PRIVATE
                 ${NEO_CORE_tests_xe_hpg_core_excludes}
                 ${CMAKE_CURRENT_SOURCE_DIR}/CMakeLists.txt
                 ${CMAKE_CURRENT_SOURCE_DIR}/command_encoder_tests_xe_hpg_core.cpp
                 ${CMAKE_CURRENT_SOURCE_DIR}/compute_mode_tests_xe_hpg_core.cpp
                 ${CMAKE_CURRENT_SOURCE_DIR}/hw_cmds_xe_hpg_core_tests.cpp
                 ${CMAKE_CURRENT_SOURCE_DIR}/gfx_core_helper_tests_xe_hpg_core.cpp
                 ${CMAKE_CURRENT_SOURCE_DIR}/image_surface_state_tests_xe_hpg_core.cpp
                 ${CMAKE_CURRENT_SOURCE_DIR}/os_agnostic_product_helper_xe_lpg_tests.cpp
                 ${CMAKE_CURRENT_SOURCE_DIR}/simd_helper_tests_xe_hpg_core.cpp
                 ${CMAKE_CURRENT_SOURCE_DIR}/test_device_caps_xe_hpg_core.cpp
                 ${CMAKE_CURRENT_SOURCE_DIR}/test_encode_dispatch_kernel_xe_hpg_core.cpp
                 ${CMAKE_CURRENT_SOURCE_DIR}/test_product_helper_xe_hpg_core.cpp
                 ${CMAKE_CURRENT_SOURCE_DIR}/test_preemption_xe_hpg_core.cpp
  )
  add_subdirectories()
endif()
