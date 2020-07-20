#
# Copyright (C) 2019-2020 Intel Corporation
#
# SPDX-License-Identifier: MIT
#

target_sources(${TARGET_NAME} PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/CMakeLists.txt
    ${CMAKE_CURRENT_SOURCE_DIR}/mock_metric.h
    ${CMAKE_CURRENT_SOURCE_DIR}/mock_metric.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/mock_metric.h
    ${CMAKE_CURRENT_SOURCE_DIR}/mock_metric.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/test_metric_enumeration.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/test_metric_query_pool.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/test_metric_tracer.cpp
)

if(UNIX)
    add_subdirectory(linux)
endif()
