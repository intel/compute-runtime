#
# Copyright (C) 2021 Intel Corporation
#
# SPDX-License-Identifier: MIT
#

set(L0_SRCS_CACHE_RESERVATION_LINUX
    ${CMAKE_CURRENT_SOURCE_DIR}/CMakeLists.txt
    ${CMAKE_CURRENT_SOURCE_DIR}${BRANCH_DIR_SUFFIX}cache_reservation_impl.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}${BRANCH_DIR_SUFFIX}cache_reservation_impl.h
)

if(UNIX)
  set_property(GLOBAL PROPERTY L0_SRCS_CACHE_RESERVATION_LINUX ${L0_SRCS_CACHE_RESERVATION_LINUX})
endif()
