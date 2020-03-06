#
# Copyright (C) 2020 Intel Corporation
#
# SPDX-License-Identifier: MIT
#

set(L0_SRCS_DLL
    ${CMAKE_CURRENT_SOURCE_DIR}/CMakeLists.txt
    ${CMAKE_CURRENT_SOURCE_DIR}/disallow_deferred_deleter.cpp
)
set_property(GLOBAL PROPERTY L0_SRCS_DLL ${L0_SRCS_DLL})