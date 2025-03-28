#
# Copyright (C) 2023-2025 Intel Corporation
#
# SPDX-License-Identifier: MIT
#

target_sources(${L0_STATIC_LIB_NAME}
               PRIVATE
               ${CMAKE_CURRENT_SOURCE_DIR}/CMakeLists.txt
               ${CMAKE_CURRENT_SOURCE_DIR}${BRANCH_DIR_SUFFIX}device_imp_helper.cpp
               ${CMAKE_CURRENT_SOURCE_DIR}/bcs_split.cpp
               ${CMAKE_CURRENT_SOURCE_DIR}/bcs_split.h
               ${CMAKE_CURRENT_SOURCE_DIR}/device.cpp
               ${CMAKE_CURRENT_SOURCE_DIR}/device.h
               ${CMAKE_CURRENT_SOURCE_DIR}/device_imp_${DRIVER_MODEL}/device_imp_${DRIVER_MODEL}.cpp
               ${CMAKE_CURRENT_SOURCE_DIR}/device_imp.cpp
               ${CMAKE_CURRENT_SOURCE_DIR}/device_imp.h
)

if(UNIX)
  target_sources(${L0_STATIC_LIB_NAME}
                 PRIVATE
                 ${CMAKE_CURRENT_SOURCE_DIR}/device_imp_drm/device_imp_peer.cpp
                 ${CMAKE_CURRENT_SOURCE_DIR}/device_imp_drm/device_imp_peer.h
  )
endif()
