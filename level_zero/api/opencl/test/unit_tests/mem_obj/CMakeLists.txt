#
# Copyright (C) 2026 Intel Corporation
#
# SPDX-License-Identifier: MIT
#

set_property(GLOBAL APPEND PROPERTY LEO_TEST_SOURCES
             ${CMAKE_CURRENT_SOURCE_DIR}/test_buffer.cpp
             ${CMAKE_CURRENT_SOURCE_DIR}/test_map_operations_handler.cpp
             ${CMAKE_CURRENT_SOURCE_DIR}/test_mem_obj_properties.cpp
)
