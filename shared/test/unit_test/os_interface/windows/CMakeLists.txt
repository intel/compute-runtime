#
# Copyright (C) 2019-2020 Intel Corporation
#
# SPDX-License-Identifier: MIT
#

set(NEO_CORE_OS_INTERFACE_TESTS_WINDOWS
  ${CMAKE_CURRENT_SOURCE_DIR}/CMakeLists.txt
  ${CMAKE_CURRENT_SOURCE_DIR}/gdi_interface_tests.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/gmm_helper_tests_win.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/mock_gdi_interface.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/mock_gdi_interface.h
  ${CMAKE_CURRENT_SOURCE_DIR}/wddm_preemption_tests.cpp
)
set_property(GLOBAL PROPERTY NEO_CORE_OS_INTERFACE_TESTS_WINDOWS ${NEO_CORE_OS_INTERFACE_TESTS_WINDOWS})
