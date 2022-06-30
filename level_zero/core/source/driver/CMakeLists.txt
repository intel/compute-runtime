#
# Copyright (C) 2020 Intel Corporation
#
# SPDX-License-Identifier: MIT
#

set(L0_SRCS_DRIVER
    ${CMAKE_CURRENT_SOURCE_DIR}/CMakeLists.txt
)

add_subdirectories()
set_property(GLOBAL PROPERTY L0_SRCS_DRIVER ${L0_SRCS_DRIVER})
