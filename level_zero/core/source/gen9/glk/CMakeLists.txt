#
# Copyright (C) 2019-2020 Intel Corporation
#
# SPDX-License-Identifier: MIT
#

if(SUPPORT_GLK)
    set(HW_SOURCES_GEN9
        ${HW_SOURCES_GEN9}
        ${CMAKE_CURRENT_SOURCE_DIR}/CMakeLists.txt
        ${CMAKE_CURRENT_SOURCE_DIR}/cmdlist_glk.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/cmdqueue_glk.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/kernel_glk.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/image_glk.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/sampler_glk.cpp
        PARENT_SCOPE
    )
endif()
