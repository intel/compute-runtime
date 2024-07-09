#
# Copyright (C) 2024 Intel Corporation
#
# SPDX-License-Identifier: MIT
#

if(TESTS_XE2_HPG_CORE)
  target_sources(${TARGET_NAME} PRIVATE
                 ${CMAKE_CURRENT_SOURCE_DIR}/CMakeLists.txt
                 ${CMAKE_CURRENT_SOURCE_DIR}/test_cmdlist_xe2_hpg_core.cpp
                 ${CMAKE_CURRENT_SOURCE_DIR}/test_cmdqueue_xe2_hpg_core.cpp
                 ${CMAKE_CURRENT_SOURCE_DIR}/test_device_xe2_hpg_core.cpp
                 ${CMAKE_CURRENT_SOURCE_DIR}/test_l0_gfx_core_helper_xe2_hpg_core.cpp
                 ${CMAKE_CURRENT_SOURCE_DIR}/test_module_xe2_hpg_core.cpp
  )
endif()
