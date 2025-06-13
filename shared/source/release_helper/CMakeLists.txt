#
# Copyright (C) 2023-2025 Intel Corporation
#
# SPDX-License-Identifier: MIT
#

set(NEO_CORE_RELEASE_HELPER
    ${CMAKE_CURRENT_SOURCE_DIR}/CMakeLists.txt
    ${CMAKE_CURRENT_SOURCE_DIR}/release_helper.h
    ${CMAKE_CURRENT_SOURCE_DIR}/release_helper.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/release_helper_base.inl
    ${CMAKE_CURRENT_SOURCE_DIR}/definitions${BRANCH_DIR_SUFFIX}release_definitions.h
    ${CMAKE_CURRENT_SOURCE_DIR}/release_helper.cpp

    ${CMAKE_CURRENT_SOURCE_DIR}/release_helper_common_xe_lpg.inl
    ${CMAKE_CURRENT_SOURCE_DIR}/release_helper_common_xe_hpg.inl
    ${CMAKE_CURRENT_SOURCE_DIR}/release_helper_1255.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/release_helper_1256.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/release_helper_1257.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/release_helper_1260.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/release_helper_1261.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/release_helper_1270.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/release_helper_1271.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/release_helper_1274.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/release_helper_2001.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/release_helper_2002.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/release_helper_2004.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/release_helper_3000.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/release_helper_3001.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/release_helper_3003.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/release_helper_common_xe2_hpg.inl
    ${CMAKE_CURRENT_SOURCE_DIR}/release_helper_common_xe3_and_later.inl
    ${CMAKE_CURRENT_SOURCE_DIR}/release_helper_common_xe3_lpg.inl
)

set_property(GLOBAL APPEND PROPERTY NEO_CORE_ENABLE_RELEASE_HELPER
             ${CMAKE_CURRENT_SOURCE_DIR}/enable_release_helper_xe.cpp
             ${CMAKE_CURRENT_SOURCE_DIR}/enable_release_helper_xe2.cpp
             ${CMAKE_CURRENT_SOURCE_DIR}${BRANCH_DIR_SUFFIX}enable_release_helper_xe3.cpp
)
set_property(GLOBAL APPEND PROPERTY NEO_CORE_RELEASE_HELPER ${NEO_CORE_RELEASE_HELPER})

add_subdirectories()
get_property(NEO_CORE_ENABLE_RELEASE_HELPER GLOBAL PROPERTY NEO_CORE_ENABLE_RELEASE_HELPER)
set_property(GLOBAL APPEND PROPERTY NEO_CORE_SRCS_LINK ${NEO_CORE_ENABLE_RELEASE_HELPER})
