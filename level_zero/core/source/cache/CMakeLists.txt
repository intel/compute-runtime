#
# Copyright (C) 2021 Intel Corporation
#
# SPDX-License-Identifier: MIT
#

set(L0_SRCS_CACHE_RESERVATION
    ${CMAKE_CURRENT_SOURCE_DIR}/CMakeLists.txt
    ${CMAKE_CURRENT_SOURCE_DIR}/cache_reservation.h
)

add_subdirectories()
set_property(GLOBAL PROPERTY L0_SRCS_CACHE_RESERVATION ${L0_SRCS_CACHE_RESERVATION})
