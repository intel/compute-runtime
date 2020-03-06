#
# Copyright (C) 2020 Intel Corporation
#
# SPDX-License-Identifier: MIT
#

set(L0_TOOLS_API
    ${CMAKE_CURRENT_SOURCE_DIR}/ze_tools_loader.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/zet_metric.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/zet_sysman.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/zet_tracing.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/zet_driver.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/zet_module.cpp
)
set_property(GLOBAL PROPERTY L0_TOOLS_API ${L0_TOOLS_API})
