#
# Copyright (C) 2023-2024 Intel Corporation
#
# SPDX-License-Identifier: MIT
#

if(WIN32)
  target_sources(${L0_STATIC_LIB_NAME}
                 PRIVATE
                 ${CMAKE_CURRENT_SOURCE_DIR}/CMakeLists.txt
                 ${CMAKE_CURRENT_SOURCE_DIR}/zes_os_sysman_imp.h
                 ${CMAKE_CURRENT_SOURCE_DIR}/sysman_kmd_sys.h
                 ${CMAKE_CURRENT_SOURCE_DIR}/sysman_kmd_sys_manager.h
                 ${CMAKE_CURRENT_SOURCE_DIR}/zes_os_sysman_imp.cpp
                 ${CMAKE_CURRENT_SOURCE_DIR}/sysman_kmd_sys_manager.cpp
                 ${CMAKE_CURRENT_SOURCE_DIR}/sysman_hw_device_id_windows.cpp
                 ${CMAKE_CURRENT_SOURCE_DIR}/zes_os_sysman_driver_imp.cpp
                 ${CMAKE_CURRENT_SOURCE_DIR}/zes_os_sysman_driver_imp.h
  )
  add_subdirectories()
endif()