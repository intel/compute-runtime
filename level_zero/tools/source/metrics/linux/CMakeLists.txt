#
# Copyright (C) 2020-2024 Intel Corporation
#
# SPDX-License-Identifier: MIT
#

if(UNIX)
  target_sources(${L0_STATIC_LIB_NAME}
                 PRIVATE
                 ${CMAKE_CURRENT_SOURCE_DIR}/CMakeLists.txt
                 ${CMAKE_CURRENT_SOURCE_DIR}/os_metric_oa_query_imp_linux.cpp
                 ${CMAKE_CURRENT_SOURCE_DIR}/os_metric_oa_enumeration_imp_linux.cpp
                 ${CMAKE_CURRENT_SOURCE_DIR}/os_metric_oa_enumeration_imp_linux.h
                 ${CMAKE_CURRENT_SOURCE_DIR}/os_metric_ip_sampling_imp_linux.cpp
  )
endif()
