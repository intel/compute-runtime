#
# Copyright (C) 2020-2022 Intel Corporation
#
# SPDX-License-Identifier: MIT
#

set(L0_TOOLS_API
    ${CMAKE_CURRENT_SOURCE_DIR}/ze_tools_loader.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/zet_debug_api_entrypoints.h
    ${CMAKE_CURRENT_SOURCE_DIR}/zet_metric_api_entrypoints.h
    ${CMAKE_CURRENT_SOURCE_DIR}/zet_module_api_entrypoints.h
    ${CMAKE_CURRENT_SOURCE_DIR}/zet_tools_all_api_entrypoints.h
)
set_property(GLOBAL PROPERTY L0_TOOLS_API ${L0_TOOLS_API})
