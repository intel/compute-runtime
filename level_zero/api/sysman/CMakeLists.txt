#
# Copyright (C) 2020-2023 Intel Corporation
#
# SPDX-License-Identifier: MIT
#

target_sources(${L0_STATIC_LIB_NAME}
               PRIVATE
               ${CMAKE_CURRENT_SOURCE_DIR}/CMakeLists.txt
               ${CMAKE_CURRENT_SOURCE_DIR}/ze_sysman_loader.cpp
               ${CMAKE_CURRENT_SOURCE_DIR}/zes_sysman_api_entrypoints.h
               ${CMAKE_CURRENT_SOURCE_DIR}/zes_sysman_all_api_entrypoints.h
               ${CMAKE_CURRENT_SOURCE_DIR}/zes_handles_struct.h
)
