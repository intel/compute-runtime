#
# Copyright (C) 2020-2025 Intel Corporation
#
# SPDX-License-Identifier: MIT
#

set(NEO_CORE_AUB
    ${CMAKE_CURRENT_SOURCE_DIR}/CMakeLists.txt
    ${CMAKE_CURRENT_SOURCE_DIR}/aub_center.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/aub_center.h
    ${CMAKE_CURRENT_SOURCE_DIR}/aub_helper.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/aub_helper.h
    ${CMAKE_CURRENT_SOURCE_DIR}/aub_helper_add_mmio.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/aub_helper_base.inl
    ${CMAKE_CURRENT_SOURCE_DIR}${BRANCH_DIR_SUFFIX}aub_helper_extra.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/aub_mapper_base.h
    ${CMAKE_CURRENT_SOURCE_DIR}/aub_subcapture.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/aub_subcapture.h
)

if(SUPPORT_XEHP_AND_LATER)
  list(APPEND NEO_CORE_AUB
       ${CMAKE_CURRENT_SOURCE_DIR}/aub_helper_xehp_and_later.inl
  )
endif()

set_property(GLOBAL PROPERTY NEO_CORE_AUB ${NEO_CORE_AUB})
add_subdirectories()
