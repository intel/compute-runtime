#
# Copyright (C) 2026 Intel Corporation
#
# SPDX-License-Identifier: MIT
#

set(LEO_OS_INTERFACE_SRCS
    ${CMAKE_CURRENT_SOURCE_DIR}/CMakeLists.txt
)

target_sources(${L0_STATIC_LIB_NAME} PRIVATE ${LEO_OS_INTERFACE_SRCS})
set_property(GLOBAL PROPERTY LEO_OS_INTERFACE_SRCS ${LEO_OS_INTERFACE_SRCS})
add_subdirectories()
