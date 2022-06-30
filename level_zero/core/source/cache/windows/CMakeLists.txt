#
# Copyright (C) 2021 Intel Corporation
#
# SPDX-License-Identifier: MIT
#

set(L0_SRCS_CACHE_RESERVATION_WINDOWS
    ${CMAKE_CURRENT_SOURCE_DIR}/CMakeLists.txt
    ${CMAKE_CURRENT_SOURCE_DIR}/cache_reservation_impl.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/cache_reservation_impl.h
)

if(WIN32)
  set_property(GLOBAL PROPERTY L0_SRCS_CACHE_RESERVATION_WINDOWS ${L0_SRCS_CACHE_RESERVATION_WINDOWS})
endif()
