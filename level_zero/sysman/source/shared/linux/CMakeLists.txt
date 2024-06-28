#
# Copyright (C) 2023-2024 Intel Corporation
#
# SPDX-License-Identifier: MIT
#

if(UNIX)
  target_sources(${L0_STATIC_LIB_NAME}
                 PRIVATE
                 ${CMAKE_CURRENT_SOURCE_DIR}/CMakeLists.txt
                 ${CMAKE_CURRENT_SOURCE_DIR}/zes_os_sysman_imp.h
                 ${CMAKE_CURRENT_SOURCE_DIR}/zes_os_sysman_imp.cpp
                 ${CMAKE_CURRENT_SOURCE_DIR}/sysman_hw_device_id_linux.cpp
                 ${CMAKE_CURRENT_SOURCE_DIR}/sysman_hw_device_id_linux.h
                 ${CMAKE_CURRENT_SOURCE_DIR}/zes_os_sysman_driver_imp.cpp
                 ${CMAKE_CURRENT_SOURCE_DIR}/zes_os_sysman_driver_imp.h
                 ${CMAKE_CURRENT_SOURCE_DIR}/sysman_fs_access_interface.cpp
                 ${CMAKE_CURRENT_SOURCE_DIR}/sysman_fs_access_interface.h
  )

  add_subdirectories()

endif()

