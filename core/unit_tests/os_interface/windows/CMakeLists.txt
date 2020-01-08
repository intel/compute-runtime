#
# Copyright (C) 2019-2020 Intel Corporation
#
# SPDX-License-Identifier: MIT
#

if(WIN32)
  set(NEO_CORE_tests_os_interface_windows
    ${CMAKE_CURRENT_SOURCE_DIR}/CMakeLists.txt
    ${CMAKE_CURRENT_SOURCE_DIR}/gdi_interface_tests.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/mock_gdi_interface.h
    ${CMAKE_CURRENT_SOURCE_DIR}/wddm_preemption_tests.cpp
  )
  set_property(GLOBAL PROPERTY NEO_CORE_tests_os_interface_windows ${NEO_CORE_tests_os_interface_windows})
endif()
