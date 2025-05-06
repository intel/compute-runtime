#
# Copyright (C) 2021-2025 Intel Corporation
#
# SPDX-License-Identifier: MIT
#

if(UNIX AND NOT DISABLE_WDDM_LINUX)
  target_sources(neo_shared_tests PRIVATE
                 ${CMAKE_CURRENT_SOURCE_DIR}/CMakeLists.txt
                 ${CMAKE_CURRENT_SOURCE_DIR}/configure_device_address_space_drm_or_wddm_test.cpp
                 ${CMAKE_CURRENT_SOURCE_DIR}/ccs_mode_drm_or_wddm_test.cpp
                 ${CMAKE_CURRENT_SOURCE_DIR}/wddm32_tests_extra.cpp
  )
endif()
