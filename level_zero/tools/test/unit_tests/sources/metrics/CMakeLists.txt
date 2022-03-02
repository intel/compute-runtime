#
# Copyright (C) 2020-2022 Intel Corporation
#
# SPDX-License-Identifier: MIT
#

target_sources(${TARGET_NAME} PRIVATE
               ${CMAKE_CURRENT_SOURCE_DIR}/CMakeLists.txt
               ${CMAKE_CURRENT_SOURCE_DIR}/mock_metric_oa.h
               ${CMAKE_CURRENT_SOURCE_DIR}/mock_metric_oa.cpp
               ${CMAKE_CURRENT_SOURCE_DIR}/mock_metric_oa_enumeration.h
               ${CMAKE_CURRENT_SOURCE_DIR}/mock_metric_oa_enumeration.cpp
               ${CMAKE_CURRENT_SOURCE_DIR}/test_metric_oa_enumeration_1.cpp
               ${CMAKE_CURRENT_SOURCE_DIR}/test_metric_oa_enumeration_2.cpp
               ${CMAKE_CURRENT_SOURCE_DIR}/test_metric_oa_query_pool_1.cpp
               ${CMAKE_CURRENT_SOURCE_DIR}/test_metric_oa_query_pool_2.cpp
               ${CMAKE_CURRENT_SOURCE_DIR}/test_metric_oa_query_pool_3.cpp
               ${CMAKE_CURRENT_SOURCE_DIR}/test_metric_oa_streamer_1.cpp
               ${CMAKE_CURRENT_SOURCE_DIR}/test_metric_oa_streamer_2.cpp
               ${CMAKE_CURRENT_SOURCE_DIR}/test_metric_oa_streamer_3.cpp
               ${CMAKE_CURRENT_SOURCE_DIR}/test_metric_oa_initialization.cpp
               ${CMAKE_CURRENT_SOURCE_DIR}/test_metric_ip_sampling_enumeration.cpp
               ${CMAKE_CURRENT_SOURCE_DIR}/metric_ip_sampling_fixture.cpp
               ${CMAKE_CURRENT_SOURCE_DIR}/metric_ip_sampling_fixture.h
               ${CMAKE_CURRENT_SOURCE_DIR}/test_metric_ip_sampling_streamer.cpp

)

add_subdirectories()
