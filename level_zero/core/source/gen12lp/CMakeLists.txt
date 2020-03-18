#
# Copyright (C) 2019-2020 Intel Corporation
#
# SPDX-License-Identifier: MIT
#

if(SUPPORT_GEN12LP)
    set(HW_SOURCES_GEN12LP
        ${CMAKE_CURRENT_SOURCE_DIR}/CMakeLists.txt
        ${CMAKE_CURRENT_SOURCE_DIR}/cmdlist_gen12lp.h
        ${CMAKE_CURRENT_SOURCE_DIR}/definitions${BRANCH_DIR_SUFFIX}/cache_flush_gen12lp.inl
        ${CMAKE_CURRENT_SOURCE_DIR}/enable_family_full_l0_gen12lp.cpp
    )
    add_subdirectories()
    target_include_directories(${L0_STATIC_LIB_NAME} PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/definitions${BRANCH_DIR_SUFFIX}/)

    target_sources(${L0_STATIC_LIB_NAME} PRIVATE ${HW_SOURCES_GEN12LP})
    set_property(GLOBAL APPEND PROPERTY L0_HW_SOURCES_GEN12LP ${HW_SOURCES_GEN12LP})
endif()
