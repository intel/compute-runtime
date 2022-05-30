#
# Copyright (C) 2020-2022 Intel Corporation
#
# SPDX-License-Identifier: MIT
#

set(L0_SYSMAN_API
    ${CMAKE_CURRENT_SOURCE_DIR}/ze_sysman_loader.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/zes_sysman_api_entrypoints.h
    ${CMAKE_CURRENT_SOURCE_DIR}/zes_sysman_all_api_entrypoints.h
)
set_property(GLOBAL PROPERTY L0_SYSMAN_API ${L0_SYSMAN_API})
