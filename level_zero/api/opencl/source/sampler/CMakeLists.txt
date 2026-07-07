#
# Copyright (C) 2026 Intel Corporation
#
# SPDX-License-Identifier: MIT
#

set(LEO_SAMPLER_SRCS
    ${CMAKE_CURRENT_SOURCE_DIR}/CMakeLists.txt
    ${CMAKE_CURRENT_SOURCE_DIR}/leo_sampler.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/leo_sampler.h
)
add_subdirectories()
target_sources(${L0_STATIC_LIB_NAME} PRIVATE ${LEO_SAMPLER_SRCS})
set_property(GLOBAL PROPERTY LEO_SAMPLER_SRCS ${LEO_SAMPLER_SRCS})
