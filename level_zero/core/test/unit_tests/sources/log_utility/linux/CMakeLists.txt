#
# Copyright (C) 2024 Intel Corporation
#
# SPDX-License-Identifier: MIT
#

if(UNIX AND DEFINED NEO_SPDLOG_DIR)
  target_sources(
                 ${TARGET_NAME}
                 PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/CMakeLists.txt
                 ${CMAKE_CURRENT_SOURCE_DIR}/core_log_manager_tests.cpp
                 ${CMAKE_CURRENT_SOURCE_DIR}/core_logging_tests.cpp
  )
endif()
