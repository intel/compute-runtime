#
# Copyright (C) 2023 Intel Corporation
#
# SPDX-License-Identifier: MIT
#

target_sources(${L0_STATIC_LIB_NAME}
               PRIVATE
               ${CMAKE_CURRENT_SOURCE_DIR}/CMakeLists.txt
               ${CMAKE_CURRENT_SOURCE_DIR}/sysman_pci.h
               ${CMAKE_CURRENT_SOURCE_DIR}/sysman_pci_imp.cpp
               ${CMAKE_CURRENT_SOURCE_DIR}/sysman_pci_imp.h
               ${CMAKE_CURRENT_SOURCE_DIR}/sysman_os_pci.h
)

add_subdirectories()
