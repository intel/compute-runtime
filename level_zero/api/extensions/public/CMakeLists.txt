#
# Copyright (C) 2020-2021 Intel Corporation
#
# SPDX-License-Identifier: MIT
#

set(L0_PUBLIC_EXTENSIONS_SRCS_API
    ${CMAKE_CURRENT_SOURCE_DIR}/CMakeLists.txt
    ${CMAKE_CURRENT_SOURCE_DIR}/ze_exp_ext.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/zet_exp_ext.cpp
)
set_property(GLOBAL PROPERTY L0_PUBLIC_EXTENSIONS_SRCS_API ${L0_PUBLIC_EXTENSIONS_SRCS_API})
