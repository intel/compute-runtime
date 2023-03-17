#
# Copyright (C) 2020-2023 Intel Corporation
#
# SPDX-License-Identifier: MIT
#

target_sources(${L0_STATIC_LIB_NAME}
               PRIVATE
               ${CMAKE_CURRENT_SOURCE_DIR}/CMakeLists.txt
               ${CMAKE_CURRENT_SOURCE_DIR}/ze_tools_loader.cpp
               ${CMAKE_CURRENT_SOURCE_DIR}/zet_debug_api_entrypoints.h
               ${CMAKE_CURRENT_SOURCE_DIR}/zet_metric_api_entrypoints.h
               ${CMAKE_CURRENT_SOURCE_DIR}/zet_module_api_entrypoints.h
               ${CMAKE_CURRENT_SOURCE_DIR}/zet_tools_all_api_entrypoints.h
)
