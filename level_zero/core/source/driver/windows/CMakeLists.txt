#
# Copyright (C) 2020 Intel Corporation
#
# SPDX-License-Identifier: MIT
#

set(L0_SRCS_DRIVER_WINDOWS
    ${CMAKE_CURRENT_SOURCE_DIR}/CMakeLists.txt
    ${CMAKE_CURRENT_SOURCE_DIR}/driver_handle_imp_windows.cpp
)

if(WIN32)
  set_property(GLOBAL PROPERTY L0_SRCS_DRIVER_WINDOWS ${L0_SRCS_DRIVER_WINDOWS})
endif()
