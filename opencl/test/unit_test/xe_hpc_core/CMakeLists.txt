#
# Copyright (C) 2021-2023 Intel Corporation
#
# SPDX-License-Identifier: MIT
#

if(TESTS_XE_HPC_CORE)
  set(IGDRCL_SRCS_tests_xe_hpc_core_excludes
      ${CMAKE_CURRENT_SOURCE_DIR}/excludes_ocl_xe_hpc_core.cpp
  )
  set_property(GLOBAL APPEND PROPERTY IGDRCL_SRCS_tests_excludes ${IGDRCL_SRCS_tests_xe_hpc_core_excludes})

  set(IGDRCL_SRCS_tests_xe_hpc_core
      ${IGDRCL_SRCS_tests_xe_hpc_core_excludes}
      ${CMAKE_CURRENT_SOURCE_DIR}/CMakeLists.txt
      ${CMAKE_CURRENT_SOURCE_DIR}/api_tests_xe_hpc_core.cpp
      ${CMAKE_CURRENT_SOURCE_DIR}/command_stream_receiver_hw_tests_xe_hpc_core.cpp
      ${CMAKE_CURRENT_SOURCE_DIR}/copy_engine_tests_xe_hpc_core.cpp
      ${CMAKE_CURRENT_SOURCE_DIR}/enqueue_tests_xe_hpc_core.cpp
      ${CMAKE_CURRENT_SOURCE_DIR}/gfx_core_helper_tests_xe_hpc_core.cpp
      ${CMAKE_CURRENT_SOURCE_DIR}/test_cl_device_caps_xe_hpc_core.cpp
      ${CMAKE_CURRENT_SOURCE_DIR}/test_cmds_programming_xe_hpc_core.cpp
      ${CMAKE_CURRENT_SOURCE_DIR}/test_platform_caps_xe_hpc_core.cpp
      ${CMAKE_CURRENT_SOURCE_DIR}/xe_hpc_core_test_ocl_fixtures.h
      ${CMAKE_CURRENT_SOURCE_DIR}/xe_hpc_core_test_ocl_fixtures.cpp
  )

  get_property(NEO_CORE_TESTS_XE_HPC_CORE GLOBAL PROPERTY NEO_CORE_TESTS_XE_HPC_CORE)
  list(APPEND IGDRCL_SRCS_tests_xe_hpc_core ${NEO_CORE_TESTS_XE_HPC_CORE})

  target_sources(igdrcl_tests PRIVATE ${IGDRCL_SRCS_tests_xe_hpc_core})
  add_subdirectories()
endif()
