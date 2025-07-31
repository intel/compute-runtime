#
# Copyright (C) 2020-2025 Intel Corporation
#
# SPDX-License-Identifier: MIT
#

target_sources(${L0_STATIC_LIB_NAME}
               PRIVATE
               ${CMAKE_CURRENT_SOURCE_DIR}/CMakeLists.txt
               ${CMAKE_CURRENT_SOURCE_DIR}/metric.cpp
               ${CMAKE_CURRENT_SOURCE_DIR}/metric.h
               ${CMAKE_CURRENT_SOURCE_DIR}/metric.inl
               ${CMAKE_CURRENT_SOURCE_DIR}/metric_oa_enumeration_imp.cpp
               ${CMAKE_CURRENT_SOURCE_DIR}/metric_oa_enumeration_imp.h
               ${CMAKE_CURRENT_SOURCE_DIR}/metric_oa_streamer_imp.cpp
               ${CMAKE_CURRENT_SOURCE_DIR}/metric_oa_streamer_imp.h
               ${CMAKE_CURRENT_SOURCE_DIR}/metric_oa_query_imp.cpp
               ${CMAKE_CURRENT_SOURCE_DIR}/metric_oa_query_imp.h
               ${CMAKE_CURRENT_SOURCE_DIR}/metric_oa_source.cpp
               ${CMAKE_CURRENT_SOURCE_DIR}/metric_oa_source.h
               ${CMAKE_CURRENT_SOURCE_DIR}/metric_oa_export_data.cpp
               ${CMAKE_CURRENT_SOURCE_DIR}/metric_oa_export_data.h
               ${CMAKE_CURRENT_SOURCE_DIR}/metric_ip_sampling_source.h
               ${CMAKE_CURRENT_SOURCE_DIR}/metric_ip_sampling_source.cpp
               ${CMAKE_CURRENT_SOURCE_DIR}/metric_ip_sampling_streamer.h
               ${CMAKE_CURRENT_SOURCE_DIR}/metric_ip_sampling_streamer.cpp
               ${CMAKE_CURRENT_SOURCE_DIR}/os_interface_metric.h
               ${CMAKE_CURRENT_SOURCE_DIR}/metric_oa_programmable_imp.cpp
               ${CMAKE_CURRENT_SOURCE_DIR}/metric_oa_programmable_imp.h
               ${CMAKE_CURRENT_SOURCE_DIR}/metric_multidevice_programmable.h
               ${CMAKE_CURRENT_SOURCE_DIR}/metric_multidevice_programmable.inl
               ${CMAKE_CURRENT_SOURCE_DIR}/metric_multidevice_programmable.cpp
               ${CMAKE_CURRENT_SOURCE_DIR}/os_metric_oa_enumeration_${DRIVER_MODEL}.cpp
               ${CMAKE_CURRENT_SOURCE_DIR}/${BRANCH_DIR_SUFFIX}/metric_device_context_create.cpp
)
add_subdirectories()
