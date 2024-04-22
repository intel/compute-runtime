#
# Copyright (C) 2024 Intel Corporation
#
# SPDX-License-Identifier: MIT
#

if(UNIX)
  target_sources(${L0_STATIC_LIB_NAME}
                 PRIVATE
                 ${CMAKE_CURRENT_SOURCE_DIR}/CMakeLists.txt
                 ${CMAKE_CURRENT_SOURCE_DIR}/linux_log_manager.h
                 ${CMAKE_CURRENT_SOURCE_DIR}/linux_log_manager.cpp
                 ${CMAKE_CURRENT_SOURCE_DIR}/linux_logging.h
                 ${CMAKE_CURRENT_SOURCE_DIR}/linux_logging.cpp
  )
endif()

