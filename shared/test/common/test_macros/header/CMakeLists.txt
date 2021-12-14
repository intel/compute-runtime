#
# Copyright (C) 2021 Intel Corporation
#
# SPDX-License-Identifier: MIT
#

add_subdirectories()
set(NEO_CORE_test_macros
    ${NEO_CORE_test_macros}
    ${CMAKE_CURRENT_SOURCE_DIR}/CMakeLists.txt
    ${CMAKE_CURRENT_SOURCE_DIR}/common_matchers.h
    ${CMAKE_CURRENT_SOURCE_DIR}/per_product_test_definitions.h
    ${CMAKE_CURRENT_SOURCE_DIR}/per_product_test_selector.h
    ${CMAKE_CURRENT_SOURCE_DIR}/per_product_test_selector_definitions.h
    PARENT_SCOPE
)
