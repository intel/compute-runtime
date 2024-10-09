#
# Copyright (C) 2020-2024 Intel Corporation
#
# SPDX-License-Identifier: MIT
#

set(L0_TESTS_TOOLS_SYSMAN_FABRICPORT_LINUX
    ${CMAKE_CURRENT_SOURCE_DIR}/CMakeLists.txt
)

if(NEO_ENABLE_I915_PRELIM_DETECTION)
  if(LIBGENL_FOUND)
    list(APPEND L0_TESTS_TOOLS_SYSMAN_FABRICPORT_LINUX
         ${CMAKE_CURRENT_SOURCE_DIR}/test_sysman_fabric_device_prelim.cpp
         ${CMAKE_CURRENT_SOURCE_DIR}/test_sysman_fabric_port_prelim.cpp
    )
  endif()
else()
  list(APPEND L0_TESTS_TOOLS_SYSMAN_FABRICPORT_LINUX
       ${CMAKE_CURRENT_SOURCE_DIR}/test_zes_fabric_port.cpp
       ${CMAKE_CURRENT_SOURCE_DIR}/mock_fabric_device.h
  )
endif()

if(UNIX)
  target_sources(${TARGET_NAME}
                 PRIVATE
                 ${L0_TESTS_TOOLS_SYSMAN_FABRICPORT_LINUX}
  )
endif()
