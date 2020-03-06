#
# Copyright (C) 2019-2020 Intel Corporation
#
# SPDX-License-Identifier: MIT
#

if(SUPPORT_GEN11)
    set(HW_SOURCES_GEN11
        ${CMAKE_CURRENT_SOURCE_DIR}/CMakeLists.txt
        ${CMAKE_CURRENT_SOURCE_DIR}/cmdlist_gen11.inl
        ${CMAKE_CURRENT_SOURCE_DIR}/enable_family_full_l0_gen11.cpp
    )
    add_subdirectories()

    target_sources(${TARGET_NAME_L0} PRIVATE ${HW_SOURCES_GEN11})
    set_property(GLOBAL PROPERTY L0_HW_SOURCES_GEN11 ${HW_SOURCES_GEN11})
endif()
