#
# Copyright (C) 2020-2024 Intel Corporation
#
# SPDX-License-Identifier: MIT
#

if(UNIX)
  target_sources(${L0_STATIC_LIB_NAME}
                 PRIVATE
                 ${CMAKE_CURRENT_SOURCE_DIR}/CMakeLists.txt
                 ${CMAKE_CURRENT_SOURCE_DIR}/sysman_os_fabric_port_imp.cpp
                 ${CMAKE_CURRENT_SOURCE_DIR}/sysman_os_fabric_port_imp.h
  )

  if(LIBGENL_FOUND)
    target_sources(${L0_STATIC_LIB_NAME}
                   PRIVATE
                   ${CMAKE_CURRENT_SOURCE_DIR}/sysman_fabric_device_access.h
                   ${CMAKE_CURRENT_SOURCE_DIR}/sysman_fabric_device_access_imp.cpp
                   ${CMAKE_CURRENT_SOURCE_DIR}/sysman_fabric_device_access_imp.h
    )
  else()
    target_sources(${L0_STATIC_LIB_NAME}
                   PRIVATE
                   ${CMAKE_CURRENT_SOURCE_DIR}/sysman_fabric_device_access_stub.cpp
                   ${CMAKE_CURRENT_SOURCE_DIR}/sysman_fabric_device_access_stub.h
    )
  endif()
endif()
