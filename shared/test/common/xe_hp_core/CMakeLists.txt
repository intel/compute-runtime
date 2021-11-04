#
# Copyright (C) 2021 Intel Corporation
#
# SPDX-License-Identifier: MIT
#

if(TESTS_XE_HP_CORE)

  set(NEO_CORE_TESTS_XE_HP_CORE
      ${CMAKE_CURRENT_SOURCE_DIR}/CMakeLists.txt
      ${CMAKE_CURRENT_SOURCE_DIR}/image_surface_state_tests_xe_hp_core.cpp
      ${CMAKE_CURRENT_SOURCE_DIR}/simd_helper_tests_xe_hp_core.cpp
      ${CMAKE_CURRENT_SOURCE_DIR}/test_encode_xe_hp_core.cpp
      ${CMAKE_CURRENT_SOURCE_DIR}/test_hw_info_config_xe_hp_core.cpp
      ${CMAKE_CURRENT_SOURCE_DIR}/test_preemption_xe_hp_core.cpp
      ${CMAKE_CURRENT_SOURCE_DIR}/test_traits_xe_hp_core.h
  )
  set_property(GLOBAL PROPERTY NEO_CORE_TESTS_XE_HP_CORE ${NEO_CORE_TESTS_XE_HP_CORE})
  add_subdirectories()

  target_sources(${TARGET_NAME} PRIVATE
                 ${COMPUTE_RUNTIME_ULT_XE_HP_CORE}
                 ${NEO_CORE_TESTS_XE_HP_CORE}
  )

endif()
