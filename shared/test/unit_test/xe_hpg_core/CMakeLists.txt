#
# Copyright (C) 2021-2022 Intel Corporation
#
# SPDX-License-Identifier: MIT
#

if(TESTS_XE_HPG_CORE)
  set(NEO_SHARED_tests_xe_hpg_core
      ${CMAKE_CURRENT_SOURCE_DIR}/CMakeLists.txt
      ${CMAKE_CURRENT_SOURCE_DIR}/compute_mode_tests_xe_hpg_core.cpp
      ${CMAKE_CURRENT_SOURCE_DIR}/excludes_xe_hpg_core.cpp
      ${CMAKE_CURRENT_SOURCE_DIR}/hw_cmds_xe_hpg_core_tests.cpp
      ${CMAKE_CURRENT_SOURCE_DIR}/hw_helper_tests_xe_hpg_core.cpp
      ${CMAKE_CURRENT_SOURCE_DIR}/test_encode_dispatch_kernel_xe_hpg_core.cpp
  )

  if(DEFINED AUB_STREAM_PROJECT_NAME)
    list(APPEND NEO_SHARED_tests_xe_hpg_core
         ${CMAKE_CURRENT_SOURCE_DIR}/aub_center_using_aubstream_lib_tests_xe_hpg_core.cpp
    )
  endif()
  target_sources(${TARGET_NAME} PRIVATE ${NEO_SHARED_tests_xe_hpg_core})
  add_subdirectories()
endif()
