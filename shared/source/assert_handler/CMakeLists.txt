#
# Copyright (C) 2023 Intel Corporation
#
# SPDX-License-Identifier: MIT
#

set(NEO_CORE_ASSERT_HANDLER
    ${CMAKE_CURRENT_SOURCE_DIR}/CMakeLists.txt
    ${CMAKE_CURRENT_SOURCE_DIR}/assert_handler.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/assert_handler.h
)

set_property(GLOBAL PROPERTY NEO_CORE_ASSERT_HANDLER ${NEO_CORE_ASSERT_HANDLER})
