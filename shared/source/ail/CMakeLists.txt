#
# Copyright (C) 2021-2022 Intel Corporation
#
# SPDX-License-Identifier: MIT
#

set(NEO_CORE_AIL
    ${CMAKE_CURRENT_SOURCE_DIR}/CMakeLists.txt
    ${CMAKE_CURRENT_SOURCE_DIR}/ail_configuration.h
    ${CMAKE_CURRENT_SOURCE_DIR}/ail_configuration.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}${BRANCH_DIR_SUFFIX}ail_configuration_extra.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/ail_configuration_base.inl
)

set_property(GLOBAL PROPERTY NEO_CORE_AIL ${NEO_CORE_AIL})
add_subdirectories()
