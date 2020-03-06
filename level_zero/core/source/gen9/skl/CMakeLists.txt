#
# Copyright (C) 2019-2020 Intel Corporation
#
# SPDX-License-Identifier: MIT
#

if(SUPPORT_SKL)
    set(HW_SOURCES_GEN9
        ${HW_SOURCES_GEN9}
        ${CMAKE_CURRENT_SOURCE_DIR}/CMakeLists.txt
        ${CMAKE_CURRENT_SOURCE_DIR}/cmdlist_skl.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/cmdqueue_skl.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/kernel_skl.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/image_skl.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/sampler_skl.cpp
        PARENT_SCOPE
    )
endif()
