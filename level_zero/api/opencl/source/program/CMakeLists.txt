#
# Copyright (C) 2026 Intel Corporation
#
# SPDX-License-Identifier: MIT
#

set(LEO_PROGRAM_SRCS
    ${CMAKE_CURRENT_SOURCE_DIR}/CMakeLists.txt
    ${CMAKE_CURRENT_SOURCE_DIR}/program.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/program.h
)
target_sources(${L0_STATIC_LIB_NAME} PRIVATE ${LEO_PROGRAM_SRCS})
set_property(GLOBAL PROPERTY LEO_PROGRAM_SRCS ${LEO_PROGRAM_SRCS})
