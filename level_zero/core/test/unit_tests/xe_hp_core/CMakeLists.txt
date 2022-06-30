#
# Copyright (C) 2021-2022 Intel Corporation
#
# SPDX-License-Identifier: MIT
#

if(TESTS_XE_HP_CORE)
  target_sources(${TARGET_NAME} PRIVATE
                 ${CMAKE_CURRENT_SOURCE_DIR}/CMakeLists.txt
                 ${CMAKE_CURRENT_SOURCE_DIR}/test_cmdlist_xe_hp_core.cpp
                 ${CMAKE_CURRENT_SOURCE_DIR}/test_cmdqueue_debugger_xe_hp_core.cpp
                 ${CMAKE_CURRENT_SOURCE_DIR}/test_cmdqueue_enqueuecommandlist_xe_hp_core.cpp
                 ${CMAKE_CURRENT_SOURCE_DIR}/test_device_xe_hp_core.cpp
                 ${CMAKE_CURRENT_SOURCE_DIR}/test_l0_hw_helper_xe_hp_core.cpp
                 ${CMAKE_CURRENT_SOURCE_DIR}/test_module_xe_hp_core.cpp
  )
  add_subdirectoriesL0(${CMAKE_CURRENT_SOURCE_DIR} "*")
endif()
