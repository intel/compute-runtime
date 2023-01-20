#
# Copyright (C) 2019-2022 Intel Corporation
#
# SPDX-License-Identifier: MIT
#

target_sources(neo_shared_tests PRIVATE
               ${CMAKE_CURRENT_SOURCE_DIR}/CMakeLists.txt
               ${CMAKE_CURRENT_SOURCE_DIR}/unified_memory_tests.cpp
)