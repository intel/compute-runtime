#
# Copyright (C) 2020-2021 Intel Corporation
#
# SPDX-License-Identifier: MIT
#

set(L0_SRCS_TOOLS_SYSMAN_LINUX
    ${CMAKE_CURRENT_SOURCE_DIR}/CMakeLists.txt
    ${CMAKE_CURRENT_SOURCE_DIR}/os_sysman_imp.h
    ${CMAKE_CURRENT_SOURCE_DIR}/os_sysman_imp.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/fs_access.cpp
)

if(UNIX)
  target_sources(${L0_STATIC_LIB_NAME}
                 PRIVATE
                 ${L0_SRCS_TOOLS_SYSMAN_LINUX}
  )
endif()

add_subdirectories()

# Make our source files visible to parent
set_property(GLOBAL PROPERTY L0_SRCS_TOOLS_SYSMAN_LINUX ${L0_SRCS_TOOLS_SYSMAN_LINUX})

if(L0_INSTALL_UDEV_RULES)
  install(
          FILES "${CMAKE_CURRENT_SOURCE_DIR}/udev/99-drm_ze_intel_gpu.rules"
          DESTINATION ${UDEV_RULES_DIR}
          COMPONENT ${PROJECT_NAME}
  )
  install(
          FILES "${CMAKE_CURRENT_SOURCE_DIR}/udev/wedged_file"
          DESTINATION "/var/lib/libze_intel_gpu/"
          COMPONENT ${PROJECT_NAME}
  )
  install(
          FILES "${CMAKE_CURRENT_SOURCE_DIR}/udev/pci_bind_status_file"
          DESTINATION "/var/lib/libze_intel_gpu/"
          COMPONENT ${PROJECT_NAME}
  )
endif()
