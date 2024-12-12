#
# Copyright (C) 2024 Intel Corporation
#
# SPDX-License-Identifier: MIT
#

set(neo_libult_common_SRCS_LIB_ULT_LINUX_XE_EUDEBUG
    ${CMAKE_CURRENT_SOURCE_DIR}/CMakeLists.txt
    ${CMAKE_CURRENT_SOURCE_DIR}/mock_eudebug_interface.h
    ${CMAKE_CURRENT_SOURCE_DIR}/mock_eudebug_interface.cpp
)
if(UNIX)
  target_sources(neo_libult_common PRIVATE ${neo_libult_common_SRCS_LIB_ULT_LINUX_XE_EUDEBUG})
endif()
set_property(GLOBAL APPEND PROPERTY neo_libult_common_SRCS_LIB_ULT_LINUX_XE_EUDEBUG ${neo_libult_common_SRCS_LIB_ULT_LINUX_XE_EUDEBUG})
add_subdirectories()

