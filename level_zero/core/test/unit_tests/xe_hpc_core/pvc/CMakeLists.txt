#
# Copyright (C) 2022 Intel Corporation
#
# SPDX-License-Identifier: MIT
#

if(TESTS_PVC)
  target_sources(${TARGET_NAME} PRIVATE
                 ${CMAKE_CURRENT_SOURCE_DIR}/CMakeLists.txt
                 ${CMAKE_CURRENT_SOURCE_DIR}/test_device_pvc.cpp
                 ${CMAKE_CURRENT_SOURCE_DIR}/test_cmdlist_pvc.cpp
                 ${CMAKE_CURRENT_SOURCE_DIR}/test_sampler_pvc.cpp
  )
endif()
