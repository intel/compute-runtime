#
# Copyright (C) 2020 Intel Corporation
#
# SPDX-License-Identifier: MIT
#

set(NEO_CORE_tests_mocks
  ${CMAKE_CURRENT_SOURCE_DIR}/CMakeLists.txt
  ${CMAKE_CURRENT_SOURCE_DIR}/mock_direct_submission_hw.h
  ${CMAKE_CURRENT_SOURCE_DIR}/mock_dispatch_kernel_encoder_interface.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/mock_dispatch_kernel_encoder_interface.h
  ${CMAKE_CURRENT_SOURCE_DIR}/mock_os_library.h
)

set_property(GLOBAL PROPERTY NEO_CORE_tests_mocks ${NEO_CORE_tests_mocks})
