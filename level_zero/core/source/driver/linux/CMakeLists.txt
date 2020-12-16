#
# Copyright (C) 2020 Intel Corporation
#
# SPDX-License-Identifier: MIT
#

set(L0_SRCS_DRIVER_LINUX
    ${CMAKE_CURRENT_SOURCE_DIR}/CMakeLists.txt
    ${CMAKE_CURRENT_SOURCE_DIR}/driver_handle_imp_linux.cpp
)

if(UNIX)
  set_property(GLOBAL PROPERTY L0_SRCS_DRIVER_LINUX ${L0_SRCS_DRIVER_LINUX})
endif()
