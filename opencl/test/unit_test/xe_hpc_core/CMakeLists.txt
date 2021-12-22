#
# Copyright (C) 2021 Intel Corporation
#
# SPDX-License-Identifier: MIT
#

if(TESTS_XE_HPC_CORE)
  set(IGDRCL_SRCS_tests_xe_hpc_core_excludes
      ${CMAKE_CURRENT_SOURCE_DIR}/excludes_xe_hpc_core.cpp
  )
  set_property(GLOBAL APPEND PROPERTY IGDRCL_SRCS_tests_excludes ${IGDRCL_SRCS_tests_xe_hpc_core_excludes})

  set(IGDRCL_SRCS_tests_xe_hpc_core
      ${IGDRCL_SRCS_tests_xe_hpc_core_excludes}
      ${CMAKE_CURRENT_SOURCE_DIR}/CMakeLists.txt
      ${CMAKE_CURRENT_SOURCE_DIR}/built_in_xe_hpc_core_tests_ocl.cpp
      ${CMAKE_CURRENT_SOURCE_DIR}/cmd_parse_tests_xe_hpc_core.cpp
      ${CMAKE_CURRENT_SOURCE_DIR}/test_cmds_programming_xe_hpc_core.cpp
      ${CMAKE_CURRENT_SOURCE_DIR}/copy_engine_tests_xe_hpc_core.cpp
      ${CMAKE_CURRENT_SOURCE_DIR}/command_stream_receiver_hw_tests_xe_hpc_core.cpp
      ${CMAKE_CURRENT_SOURCE_DIR}/dispatch_walker_tests_xe_hpc_core.cpp
      ${CMAKE_CURRENT_SOURCE_DIR}/enqueue_tests_xe_hpc_core.cpp
      ${CMAKE_CURRENT_SOURCE_DIR}/hw_helper_tests_xe_hpc_core.cpp
      ${CMAKE_CURRENT_SOURCE_DIR}/test_device_caps_xe_hpc_core.cpp
      ${CMAKE_CURRENT_SOURCE_DIR}/test_platform_caps_xe_hpc_core.cpp
      ${CMAKE_CURRENT_SOURCE_DIR}/test_sample_xe_hpc_core.cpp
      ${CMAKE_CURRENT_SOURCE_DIR}/test_preamble_xe_hpc_core.cpp
      ${CMAKE_CURRENT_SOURCE_DIR}/api_tests_xe_hpc_core.cpp
  )

  if(DEFINED AUB_STREAM_PROJECT_NAME)
    list(APPEND IGDRCL_SRCS_tests_xe_hpc_core
         ${CMAKE_CURRENT_SOURCE_DIR}/aub_center_using_aubstream_lib_tests_xe_hpc_core.cpp
    )
  endif()

  get_property(NEO_CORE_TESTS_XE_HPC_CORE GLOBAL PROPERTY NEO_CORE_TESTS_XE_HPC_CORE)
  list(APPEND IGDRCL_SRCS_tests_xe_hpc_core ${NEO_CORE_TESTS_XE_HPC_CORE})

  target_sources(igdrcl_tests PRIVATE ${IGDRCL_SRCS_tests_xe_hpc_core})
  add_subdirectories()
endif()
