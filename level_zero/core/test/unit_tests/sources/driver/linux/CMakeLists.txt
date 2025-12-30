#
# Copyright (C) 2020-2026 Intel Corporation
#
# SPDX-License-Identifier: MIT
#

if(UNIX)
  target_sources(${TARGET_NAME} PRIVATE
                 ${CMAKE_CURRENT_SOURCE_DIR}/CMakeLists.txt
                 ${CMAKE_CURRENT_SOURCE_DIR}/test_driver_handle_imp_linux.cpp
                 ${CMAKE_CURRENT_SOURCE_DIR}/test_driver_handle_ipc_socket.cpp
  )
endif()
