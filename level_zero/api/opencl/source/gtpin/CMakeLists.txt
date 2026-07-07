#
# Copyright (C) 2026 Intel Corporation
#
# SPDX-License-Identifier: MIT
#

set(LEO_GTPIN_SRCS
    ${CMAKE_CURRENT_SOURCE_DIR}/CMakeLists.txt
    ${CMAKE_CURRENT_SOURCE_DIR}/leo_gtpin_notify.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/leo_gtpin_notify.h
)
target_sources(${L0_STATIC_LIB_NAME} PRIVATE ${LEO_GTPIN_SRCS})
set_property(GLOBAL PROPERTY LEO_GTPIN_SRCS ${LEO_GTPIN_SRCS})
