#
# Copyright (C) 2019-2020 Intel Corporation
#
# SPDX-License-Identifier: MIT
#

set(L0_SRCS_TOOLS_PIN
    ${CMAKE_CURRENT_SOURCE_DIR}/pin.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/pin.h
)

target_include_directories(${TARGET_NAME_L0}
    PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/
)

target_sources(${TARGET_NAME_L0} PRIVATE
    ${L0_SRCS_TOOLS_PIN}
)

# Make our source files visible to parent
set_property(GLOBAL PROPERTY L0_SRCS_TOOLS_PIN ${L0_SRCS_TOOLS_PIN})
