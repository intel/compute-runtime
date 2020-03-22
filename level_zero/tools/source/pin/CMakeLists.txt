#
# Copyright (C) 2019-2020 Intel Corporation
#
# SPDX-License-Identifier: MIT
#

set(L0_SRCS_TOOLS_PIN
    ${CMAKE_CURRENT_SOURCE_DIR}/CMakeLists.txt
    ${CMAKE_CURRENT_SOURCE_DIR}/pin.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/pin.h
)

target_sources(${L0_STATIC_LIB_NAME} PRIVATE
    ${L0_SRCS_TOOLS_PIN}
)

# Make our source files visible to parent
set_property(GLOBAL PROPERTY L0_SRCS_TOOLS_PIN ${L0_SRCS_TOOLS_PIN})
