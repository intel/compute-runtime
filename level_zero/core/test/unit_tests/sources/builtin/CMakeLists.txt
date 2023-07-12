#
# Copyright (C) 2020-2023 Intel Corporation
#
# SPDX-License-Identifier: MIT
#

target_sources(${TARGET_NAME} PRIVATE
               ${CMAKE_CURRENT_SOURCE_DIR}/CMakeLists.txt
               ${CMAKE_CURRENT_SOURCE_DIR}/create_ult_builtin_functions_lib.cpp
               ${CMAKE_CURRENT_SOURCE_DIR}/builtin_functions_tests.cpp
)
