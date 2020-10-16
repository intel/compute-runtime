#
# Copyright (C) 2018-2020 Intel Corporation
#
# SPDX-License-Identifier: MIT
#

set(NEO_CORE_AUB
    ${CMAKE_CURRENT_SOURCE_DIR}/CMakeLists.txt
    ${CMAKE_CURRENT_SOURCE_DIR}/aub_center.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/aub_center.h
    ${CMAKE_CURRENT_SOURCE_DIR}${BRANCH_DIR_SUFFIX}/aub_helper.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/aub_helper.h
    ${CMAKE_CURRENT_SOURCE_DIR}/aub_helper_add_mmio.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/aub_helper_base.inl
    ${CMAKE_CURRENT_SOURCE_DIR}/aub_helper_bdw_plus.inl
    ${CMAKE_CURRENT_SOURCE_DIR}/aub_mapper_base.h
    ${CMAKE_CURRENT_SOURCE_DIR}/aub_stream_provider.h
    ${CMAKE_CURRENT_SOURCE_DIR}/aub_subcapture.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/aub_subcapture.h
)
set_property(GLOBAL PROPERTY NEO_CORE_AUB ${NEO_CORE_AUB})
add_subdirectories()
