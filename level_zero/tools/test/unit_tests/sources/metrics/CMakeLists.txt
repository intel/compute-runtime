#
# Copyright (C) 2020-2024 Intel Corporation
#
# SPDX-License-Identifier: MIT
#

target_sources(${TARGET_NAME} PRIVATE
               ${CMAKE_CURRENT_SOURCE_DIR}/CMakeLists.txt
               ${CMAKE_CURRENT_SOURCE_DIR}/metric_ip_sampling_fixture.cpp
               ${CMAKE_CURRENT_SOURCE_DIR}/${BRANCH_DIR_SUFFIX}/metric_ip_sampling_fixture.h
               ${CMAKE_CURRENT_SOURCE_DIR}/metric_query_pool_fixture.cpp
               ${CMAKE_CURRENT_SOURCE_DIR}/metric_query_pool_fixture.h
               ${CMAKE_CURRENT_SOURCE_DIR}/mock_metric_ip_sampling_source.h
               ${CMAKE_CURRENT_SOURCE_DIR}/mock_metric_oa.h
               ${CMAKE_CURRENT_SOURCE_DIR}/mock_metric_source.h
               ${CMAKE_CURRENT_SOURCE_DIR}/mock_metric_oa.cpp
               ${CMAKE_CURRENT_SOURCE_DIR}/mock_metric_oa_enumeration.h
               ${CMAKE_CURRENT_SOURCE_DIR}/mock_metric_oa_enumeration.cpp
               ${CMAKE_CURRENT_SOURCE_DIR}/test_metric_oa_enumeration_1.cpp
               ${CMAKE_CURRENT_SOURCE_DIR}/test_metric_oa_enumeration_2.cpp
               ${CMAKE_CURRENT_SOURCE_DIR}/test_metric_oa_query_pool_1.cpp
               ${CMAKE_CURRENT_SOURCE_DIR}/test_metric_oa_query_pool_2.cpp
               ${CMAKE_CURRENT_SOURCE_DIR}/test_metric_oa_query_pool_3.cpp
               ${CMAKE_CURRENT_SOURCE_DIR}/test_metric_oa_query_pool_4.cpp
               ${CMAKE_CURRENT_SOURCE_DIR}/test_metric_oa_query_pool_5.cpp
               ${CMAKE_CURRENT_SOURCE_DIR}/test_metric_oa_streamer_1.cpp
               ${CMAKE_CURRENT_SOURCE_DIR}/test_metric_oa_streamer_2.cpp
               ${CMAKE_CURRENT_SOURCE_DIR}/test_metric_oa_streamer_3.cpp
               ${CMAKE_CURRENT_SOURCE_DIR}/test_metric_oa_initialization.cpp
               ${CMAKE_CURRENT_SOURCE_DIR}/mock_metric_oa_programmable.h
               ${CMAKE_CURRENT_SOURCE_DIR}/mock_metric_oa_programmable.cpp
               ${CMAKE_CURRENT_SOURCE_DIR}/test_metric_group.cpp
               ${CMAKE_CURRENT_SOURCE_DIR}/test_metric_ip_sampling_enumeration.cpp
               ${CMAKE_CURRENT_SOURCE_DIR}/test_metric_ip_sampling_streamer.cpp
               ${CMAKE_CURRENT_SOURCE_DIR}/test_metric_oa_export.cpp
               ${CMAKE_CURRENT_SOURCE_DIR}/test_metric_programmable.cpp
               ${CMAKE_CURRENT_SOURCE_DIR}/test_metric_concurrent_groups.cpp
               ${CMAKE_CURRENT_SOURCE_DIR}/test_metric.cpp
               ${CMAKE_CURRENT_SOURCE_DIR}/test_multidevice_metric_programmable.cpp
               ${CMAKE_CURRENT_SOURCE_DIR}/${BRANCH_DIR_SUFFIX}/test_metric_tracer.cpp
)

add_subdirectories()
