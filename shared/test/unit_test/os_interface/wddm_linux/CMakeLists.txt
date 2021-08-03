#
# Copyright (C) 2021 Intel Corporation
#
# SPDX-License-Identifier: MIT
#

set(NEO_CORE_OS_INTERFACE_TESTS_WDDM_LINUX
    ${CMAKE_CURRENT_SOURCE_DIR}/CMakeLists.txt
    ${CMAKE_CURRENT_SOURCE_DIR}/configure_device_address_space_drm_or_wddm_test.cpp
)

set_property(GLOBAL PROPERTY NEO_CORE_OS_INTERFACE_TESTS_WDDM_LINUX ${NEO_CORE_OS_INTERFACE_TESTS_WDDM_LINUX})

if(UNIX AND NOT DISABLE_WDDM_LINUX)
  target_sources(${TARGET_NAME} PRIVATE
                 ${NEO_CORE_OS_INTERFACE_TESTS_WDDM_LINUX}
  )
endif()
