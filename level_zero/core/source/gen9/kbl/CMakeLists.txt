#
# Copyright (C) 2019-2020 Intel Corporation
#
# SPDX-License-Identifier: MIT
#

if(SUPPORT_KBL)
    set(HW_SOURCES_GEN9
        ${HW_SOURCES_GEN9}
        ${CMAKE_CURRENT_SOURCE_DIR}/CMakeLists.txt
        ${CMAKE_CURRENT_SOURCE_DIR}/cmdlist_kbl.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/cmdqueue_kbl.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/kernel_kbl.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/image_kbl.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/sampler_kbl.cpp
        PARENT_SCOPE
    )
endif()
