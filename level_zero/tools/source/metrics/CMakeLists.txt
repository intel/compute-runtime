#
# Copyright (C) 2020-2022 Intel Corporation
#
# SPDX-License-Identifier: MIT
#

set(L0_SRCS_TOOLS_METRICS
)

list(APPEND L0_SRCS_TOOLS_METRICS
     ${CMAKE_CURRENT_SOURCE_DIR}/metric.cpp
     ${CMAKE_CURRENT_SOURCE_DIR}/metric_oa_enumeration_imp.cpp
     ${CMAKE_CURRENT_SOURCE_DIR}/metric_oa_enumeration_imp.h
     ${CMAKE_CURRENT_SOURCE_DIR}/metric_oa_streamer_imp.cpp
     ${CMAKE_CURRENT_SOURCE_DIR}/metric_oa_streamer_imp.h
     ${CMAKE_CURRENT_SOURCE_DIR}/metric_oa_query_imp.cpp
     ${CMAKE_CURRENT_SOURCE_DIR}/metric_oa_query_imp.h
     ${CMAKE_CURRENT_SOURCE_DIR}/metric_oa_source.cpp
     ${CMAKE_CURRENT_SOURCE_DIR}/metric_oa_source.h
     ${CMAKE_CURRENT_SOURCE_DIR}/os_metric_ip_sampling.h
     ${CMAKE_CURRENT_SOURCE_DIR}/metric_ip_sampling_source.h
     ${CMAKE_CURRENT_SOURCE_DIR}/metric_ip_sampling_source.cpp
     ${CMAKE_CURRENT_SOURCE_DIR}/metric_ip_sampling_streamer.h
     ${CMAKE_CURRENT_SOURCE_DIR}/metric_ip_sampling_streamer.cpp
)

if(UNIX)
  add_subdirectory(linux)
else()
  add_subdirectory(windows)
endif()

target_sources(${L0_STATIC_LIB_NAME}
               PRIVATE
               ${L0_SRCS_TOOLS_METRICS}
               ${CMAKE_CURRENT_SOURCE_DIR}/CMakeLists.txt
)

# Make our source files visible to parent
set_property(GLOBAL PROPERTY L0_SRCS_TOOLS_METRICS ${L0_SRCS_TOOLS_METRICS})
