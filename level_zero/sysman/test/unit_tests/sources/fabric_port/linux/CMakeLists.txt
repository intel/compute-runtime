#
# Copyright (C) 2020-2024 Intel Corporation
#
# SPDX-License-Identifier: MIT
#

if(LIBGENL_FOUND)
  set(L0_TESTS_SYSMAN_FABRICPORT_LINUX
      ${CMAKE_CURRENT_SOURCE_DIR}/CMakeLists.txt
      ${CMAKE_CURRENT_SOURCE_DIR}/test_sysman_fabric_device.cpp
      ${CMAKE_CURRENT_SOURCE_DIR}/test_sysman_fabric_port.cpp
  )
endif()

if(UNIX)
  target_sources(${TARGET_NAME}
                 PRIVATE
                 ${L0_TESTS_SYSMAN_FABRICPORT_LINUX}
  )
endif()
