#
# Copyright (C) 2026 Intel Corporation
#
# SPDX-License-Identifier: MIT
#

set_property(GLOBAL APPEND PROPERTY LEO_TEST_SOURCES
             ${CMAKE_CURRENT_SOURCE_DIR}/test_cl_device.cpp
             ${CMAKE_CURRENT_SOURCE_DIR}/test_cl_device_diagnostics_extension.cpp
)
