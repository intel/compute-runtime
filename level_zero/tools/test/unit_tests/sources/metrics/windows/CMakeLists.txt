#
# Copyright (C) 2021-2024 Intel Corporation
#
# SPDX-License-Identifier: MIT
#

if(WIN32)
  target_sources(${TARGET_NAME} PRIVATE
                 ${CMAKE_CURRENT_SOURCE_DIR}/CMakeLists.txt
                 ${CMAKE_CURRENT_SOURCE_DIR}/test_metric_ip_sampling.cpp
                 ${CMAKE_CURRENT_SOURCE_DIR}/test_metric_oa_query_pool_windows.cpp
  )
endif()
