#
# Copyright (C) 2026 Intel Corporation
#
# SPDX-License-Identifier: MIT
#

set_property(GLOBAL APPEND PROPERTY LEO_TEST_SOURCES
             ${CMAKE_CURRENT_SOURCE_DIR}/test_base_object.cpp
             ${CMAKE_CURRENT_SOURCE_DIR}/test_cl_validators.cpp
             ${CMAKE_CURRENT_SOURCE_DIR}/test_get_info_status_mapper.cpp
             ${CMAKE_CURRENT_SOURCE_DIR}/test_l0_to_cl_return_types_mapper.cpp
)
