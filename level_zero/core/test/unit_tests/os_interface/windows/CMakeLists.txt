#
# Copyright (C) 2021 Intel Corporation
#
# SPDX-License-Identifier: MIT
#

set(NEO_L0_OS_INTERFACE_TESTS_WINDOWS
    ${CMAKE_CURRENT_SOURCE_DIR}/CMakeLists.txt
    ${CMAKE_CURRENT_SOURCE_DIR}/wddm_l0_tests.cpp
)

set_property(GLOBAL PROPERTY NEO_L0_OS_INTERFACE_TESTS_WINDOWS ${NEO_L0_OS_INTERFACE_TESTS_WINDOWS})

if(WIN32)
  target_sources(${TARGET_NAME} PRIVATE
                 ${NEO_L0_OS_INTERFACE_TESTS_WINDOWS}
  )
endif()
