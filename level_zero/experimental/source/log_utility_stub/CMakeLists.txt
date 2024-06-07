#
# Copyright (C) 2024 Intel Corporation
#
# SPDX-License-Identifier: MIT
#

if(NOT DEFINED NEO_SPDLOG_DIR)
  target_sources(${L0_STATIC_LIB_NAME}
                 PRIVATE
                 ${CMAKE_CURRENT_SOURCE_DIR}/CMakeLists.txt
                 ${CMAKE_CURRENT_SOURCE_DIR}/log_manager.h
                 ${CMAKE_CURRENT_SOURCE_DIR}/log_manager_stub.cpp
  )
endif()
