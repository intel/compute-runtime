#
# Copyright (C) 2025 Intel Corporation
#
# SPDX-License-Identifier: MIT
#

if(TESTS_XE3_CORE)
  set(IGDRCL_SRCS_tests_xe3_core_excludes
      ${CMAKE_CURRENT_SOURCE_DIR}/excludes_ocl_xe3_core.cpp
  )
  set_property(GLOBAL APPEND PROPERTY IGDRCL_SRCS_tests_excludes ${IGDRCL_SRCS_tests_xe3_core_excludes})

  set(IGDRCL_SRCS_tests_xe3_core
      ${IGDRCL_SRCS_tests_xe3_core_excludes}
      ${CMAKE_CURRENT_SOURCE_DIR}/CMakeLists.txt
      ${CMAKE_CURRENT_SOURCE_DIR}/cl_gfx_core_helper_tests_xe3_core.cpp
      ${CMAKE_CURRENT_SOURCE_DIR}/cmd_parse_tests_xe3_core.cpp
      ${CMAKE_CURRENT_SOURCE_DIR}/command_stream_receiver_hw_tests_xe3_core.cpp
      ${CMAKE_CURRENT_SOURCE_DIR}/copy_engine_tests_xe3_core.cpp
      ${CMAKE_CURRENT_SOURCE_DIR}/device_info_tests_xe3_core.cpp
      ${CMAKE_CURRENT_SOURCE_DIR}/dispatch_walker_tests_xe3_core.cpp
      ${CMAKE_CURRENT_SOURCE_DIR}/test_cmds_programming_xe3_core.cpp
      ${CMAKE_CURRENT_SOURCE_DIR}/test_cl_device_caps_xe3_core.cpp
      ${CMAKE_CURRENT_SOURCE_DIR}/test_platform_caps_xe3_core.cpp
      ${CMAKE_CURRENT_SOURCE_DIR}/test_sample_xe3_core.cpp
  )

  get_property(NEO_CORE_TESTS_XE3_CORE GLOBAL PROPERTY NEO_CORE_TESTS_XE3_CORE)
  list(APPEND IGDRCL_SRCS_tests_xe3_core ${NEO_CORE_TESTS_XE3_CORE})

  target_sources(igdrcl_tests PRIVATE ${IGDRCL_SRCS_tests_xe3_core})
  add_subdirectories()
endif()
