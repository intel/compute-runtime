#
# Copyright (C) 2024 Intel Corporation
#
# SPDX-License-Identifier: MIT
#

if(WIN32 OR NOT DISABLE_WDDM_LINUX)
  target_sources(${L0_STATIC_LIB_NAME}
                 PRIVATE
                 ${CMAKE_CURRENT_SOURCE_DIR}/CMakeLists.txt
                 ${CMAKE_CURRENT_SOURCE_DIR}/os_metric_oa_enumeration_imp_wddm.cpp
                 ${CMAKE_CURRENT_SOURCE_DIR}/os_metric_oa_enumeration_imp_wddm.h
  )
endif()
