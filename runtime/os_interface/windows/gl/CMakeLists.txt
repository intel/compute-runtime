#
# Copyright (C) 2017-2018 Intel Corporation
#
# SPDX-License-Identifier: MIT
#

set(RUNTIME_SRCS_OS_INTERFACE_WINDOWS_GL
  ${CMAKE_CURRENT_SOURCE_DIR}/CMakeLists.txt
  ${CMAKE_CURRENT_SOURCE_DIR}/gl_arb_sync_event_os.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/gl_sharing_win.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/gl_options.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/gl_sharing_os.h
)
if(WIN32)
  target_sources(${NEO_STATIC_LIB_NAME} PRIVATE ${RUNTIME_SRCS_OS_INTERFACE_WINDOWS_GL})
endif()
