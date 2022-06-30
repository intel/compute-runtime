#
# Copyright (C) 2021-2022 Intel Corporation
#
# SPDX-License-Identifier: MIT
#

if(TESTS_XE_HP_CORE)
  set(IGDRCL_SRCS_tests_xe_hp_core_excludes
      ${CMAKE_CURRENT_SOURCE_DIR}/excludes_ocl_xe_hp_core.cpp
  )

  set_property(GLOBAL APPEND PROPERTY IGDRCL_SRCS_tests_excludes ${IGDRCL_SRCS_tests_xe_hp_core_excludes})

  set(IGDRCL_SRCS_tests_xe_hp_core
      ${IGDRCL_SRCS_tests_xe_hp_core_excludes}
      ${CMAKE_CURRENT_SOURCE_DIR}/CMakeLists.txt
      ${CMAKE_CURRENT_SOURCE_DIR}/copy_engine_tests_xe_hp_core.cpp
      ${CMAKE_CURRENT_SOURCE_DIR}/test_device_caps_xe_hp_core.cpp
      ${CMAKE_CURRENT_SOURCE_DIR}/hw_helper_tests_xe_hp_core.cpp
      ${CMAKE_CURRENT_SOURCE_DIR}/test_platform_caps_xe_hp_core.cpp
      ${CMAKE_CURRENT_SOURCE_DIR}/test_sample_xe_hp_core.cpp
  )

  target_sources(igdrcl_tests PRIVATE ${IGDRCL_SRCS_tests_xe_hp_core})
  add_subdirectories()
endif()
