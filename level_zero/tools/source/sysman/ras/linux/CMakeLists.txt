#
# Copyright (C) 2020 Intel Corporation
#
# SPDX-License-Identifier: MIT
#

set(L0_SRCS_TOOLS_SYSMAN_RAS_LINUX
    ${CMAKE_CURRENT_SOURCE_DIR}/os_ras_imp.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/os_ras_imp.h
)

if(UNIX)
  target_sources(${L0_STATIC_LIB_NAME}
                 PRIVATE
                 ${L0_SRCS_TOOLS_SYSMAN_RAS_LINUX}
  )
endif()

# Make our source files visible to parent
set_property(GLOBAL PROPERTY L0_SRCS_TOOLS_SYSMAN_RAS_LINUX ${L0_SRCS_TOOLS_SYSMAN_RAS_LINUX})

if(L0_INSTALL_UDEV_RULES)
  install(
          FILES "${CMAKE_CURRENT_SOURCE_DIR}${BRANCH_DIR_SUFFIX}/udev/99-drm_ze_intel_gpu.rules"
          DESTINATION ${UDEV_RULES_DIR}
          COMPONENT ${PROJECT_NAME}
  )
endif()

if(L0_INSTALL_UDEV_RULES)
  foreach(counter IN ITEMS
            ras_reset_count
            ras_cache_error_uncorrectable
            ras_cache_error_correctable
    )
    install(
            FILES "${CMAKE_CURRENT_SOURCE_DIR}${BRANCH_DIR_SUFFIX}/udev/counter_init"
            DESTINATION "/var/lib/libze_intel_gpu/"
            COMPONENT ${PROJECT_NAME}
            RENAME ${counter}
    )
  endforeach()
endif()
