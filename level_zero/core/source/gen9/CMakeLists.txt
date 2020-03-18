#
# Copyright (C) 2019-2020 Intel Corporation
#
# SPDX-License-Identifier: MIT
#

if(SUPPORT_GEN9)
    set(HW_SOURCES_GEN9
        ${CMAKE_CURRENT_SOURCE_DIR}/CMakeLists.txt
        ${CMAKE_CURRENT_SOURCE_DIR}/cmdlist_gen9.inl
        ${CMAKE_CURRENT_SOURCE_DIR}/enable_family_full_l0_gen9.cpp
    )

    add_subdirectories()

    target_sources(${L0_STATIC_LIB_NAME} PRIVATE ${HW_SOURCES_GEN9})
    set_property(GLOBAL PROPERTY L0_HW_SOURCES_GEN9 ${HW_SOURCES_GEN9})
endif()
