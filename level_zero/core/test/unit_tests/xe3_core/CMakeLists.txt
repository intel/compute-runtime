#
# Copyright (C) 2025 Intel Corporation
#
# SPDX-License-Identifier: MIT
#

if(TESTS_XE3_CORE)
  target_sources(${TARGET_NAME} PRIVATE
                 ${CMAKE_CURRENT_SOURCE_DIR}/CMakeLists.txt
                 ${CMAKE_CURRENT_SOURCE_DIR}/test_cmdlist_xe3_core.cpp
                 ${CMAKE_CURRENT_SOURCE_DIR}/test_device_xe3_core.cpp
                 ${CMAKE_CURRENT_SOURCE_DIR}/test_l0_gfx_core_helper_xe3_core.cpp
                 ${CMAKE_CURRENT_SOURCE_DIR}/test_module_xe3_core.cpp
  )
endif()
