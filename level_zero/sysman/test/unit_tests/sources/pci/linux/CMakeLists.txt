#
# Copyright (C) 2023 Intel Corporation
#
# SPDX-License-Identifier: MIT
#

set(L0_TESTS_SYSMAN_PCI_LINUX
    ${CMAKE_CURRENT_SOURCE_DIR}/CMakeLists.txt
)

list(APPEND L0_TESTS_SYSMAN_PCI_LINUX
     ${CMAKE_CURRENT_SOURCE_DIR}/test_zes_pci.cpp
     ${CMAKE_CURRENT_SOURCE_DIR}/mock_sysfs_pci.h
)

if(UNIX)
  target_sources(${TARGET_NAME}
                 PRIVATE
                 ${L0_TESTS_SYSMAN_PCI_LINUX}
  )
endif()
