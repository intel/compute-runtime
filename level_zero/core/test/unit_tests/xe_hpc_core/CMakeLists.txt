#
# Copyright (C) 2021-2023 Intel Corporation
#
# SPDX-License-Identifier: MIT
#

if(TESTS_XE_HPC_CORE)
  target_sources(${TARGET_NAME} PRIVATE
                 ${CMAKE_CURRENT_SOURCE_DIR}/CMakeLists.txt
                 ${CMAKE_CURRENT_SOURCE_DIR}/test_cmdlist_xe_hpc_core.cpp
                 ${CMAKE_CURRENT_SOURCE_DIR}/test_cmdqueue_xe_hpc_core.cpp
                 ${CMAKE_CURRENT_SOURCE_DIR}/test_device_xe_hpc_core.cpp
                 ${CMAKE_CURRENT_SOURCE_DIR}/test_l0_gfx_core_helper_xe_hpc_core.cpp
                 ${CMAKE_CURRENT_SOURCE_DIR}/test_module_xe_hpc_core.cpp
                 ${CMAKE_CURRENT_SOURCE_DIR}/xe_hpc_core_test_l0_fixtures.h
                 ${CMAKE_CURRENT_SOURCE_DIR}/xe_hpc_core_test_l0_fixtures.cpp
                 ${CMAKE_CURRENT_SOURCE_DIR}/test_event_xe_hpc_core.cpp
  )
  add_subdirectories()
endif()
