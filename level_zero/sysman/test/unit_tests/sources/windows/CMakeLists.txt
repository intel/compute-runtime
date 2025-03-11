#
# Copyright (C) 2020-2025 Intel Corporation
#
# SPDX-License-Identifier: MIT
#

if(WIN32)
  target_sources(${TARGET_NAME}
                 PRIVATE
                 ${CMAKE_CURRENT_SOURCE_DIR}/CMakeLists.txt
                 ${CMAKE_CURRENT_SOURCE_DIR}/mock_sysman_wddm.h
                 ${CMAKE_CURRENT_SOURCE_DIR}/mock_kmd_sys_manager.h
                 ${CMAKE_CURRENT_SOURCE_DIR}/mock_sysman_fixture.h
                 ${CMAKE_CURRENT_SOURCE_DIR}/mock_sysman_driver.h
                 ${CMAKE_CURRENT_SOURCE_DIR}/test_sysman_driver.cpp
                 ${CMAKE_CURRENT_SOURCE_DIR}/test_sysman_manager.cpp
                 ${CMAKE_CURRENT_SOURCE_DIR}/test_sysman.cpp
                 ${CMAKE_CURRENT_SOURCE_DIR}/test_sysman_core_handle_support.cpp
  )
endif()
add_subdirectories()
