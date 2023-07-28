#
# Copyright (C) 2021-2023 Intel Corporation
#
# SPDX-License-Identifier: MIT
#

set(NEO_CORE_test_macros
    ${CMAKE_CURRENT_SOURCE_DIR}/CMakeLists.txt
    ${CMAKE_CURRENT_SOURCE_DIR}/test.h
)
add_subdirectories()
set_property(GLOBAL PROPERTY NEO_CORE_test_macros ${NEO_CORE_test_macros})
