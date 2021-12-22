#
# Copyright (C) 2020-2021 Intel Corporation
#
# SPDX-License-Identifier: MIT
#

target_sources(${TARGET_NAME} PRIVATE
               ${CMAKE_CURRENT_SOURCE_DIR}/CMakeLists.txt
               ${CMAKE_CURRENT_SOURCE_DIR}/mock_metric.h
               ${CMAKE_CURRENT_SOURCE_DIR}/mock_metric.cpp
               ${CMAKE_CURRENT_SOURCE_DIR}/mock_metric_enumeration.h
               ${CMAKE_CURRENT_SOURCE_DIR}/mock_metric_enumeration.cpp
               ${CMAKE_CURRENT_SOURCE_DIR}/test_metric_enumeration_1.cpp
               ${CMAKE_CURRENT_SOURCE_DIR}/test_metric_enumeration_2.cpp
               ${CMAKE_CURRENT_SOURCE_DIR}/test_metric_query_pool_1.cpp
               ${CMAKE_CURRENT_SOURCE_DIR}/test_metric_query_pool_2.cpp
               ${CMAKE_CURRENT_SOURCE_DIR}/test_metric_query_pool_3.cpp
               ${CMAKE_CURRENT_SOURCE_DIR}/test_metric_streamer_1.cpp
               ${CMAKE_CURRENT_SOURCE_DIR}/test_metric_streamer_2.cpp
               ${CMAKE_CURRENT_SOURCE_DIR}/test_metric_streamer_3.cpp

)

add_subdirectories()
