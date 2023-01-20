#
# Copyright (C) 2022 Intel Corporation
#
# SPDX-License-Identifier: MIT
#

target_sources(neo_shared_tests PRIVATE
               ${CMAKE_CURRENT_SOURCE_DIR}/CMakeLists.txt
               ${CMAKE_CURRENT_SOURCE_DIR}/debug_manager.cpp
               ${CMAKE_CURRENT_SOURCE_DIR}/debug_settings_manager_tests.cpp
)

if(WIN32)
  target_sources(neo_shared_tests PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/windows/debug_variables_helper_windows_tests.cpp)
elseif(UNIX)
  target_sources(neo_shared_tests PRIVATE
                 ${CMAKE_CURRENT_SOURCE_DIR}/linux/debug_settings_manager_linux_tests.cpp
                 ${CMAKE_CURRENT_SOURCE_DIR}/linux/debug_variables_helper_linux_tests.cpp
  )
endif()