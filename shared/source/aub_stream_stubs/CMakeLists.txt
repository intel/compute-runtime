#
# Copyright (C) 2024 Intel Corporation
#
# SPDX-License-Identifier: MIT
#

if(NOT DEFINED AUB_STREAM_PROJECT_NAME)
  add_library(aub_stream_stubs OBJECT EXCLUDE_FROM_ALL
              ${CMAKE_CURRENT_SOURCE_DIR}/aub_stream_interface_stub.cpp
              ${CMAKE_CURRENT_SOURCE_DIR}/aub_stream_stubs.cpp
  )
  target_include_directories(aub_stream_stubs PRIVATE
                             ${AUB_STREAM_HEADERS_DIR}
  )
endif()
