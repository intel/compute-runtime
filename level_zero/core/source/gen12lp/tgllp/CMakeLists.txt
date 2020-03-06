#
# Copyright (C) 2019-2020 Intel Corporation
#
# SPDX-License-Identifier: MIT
#

if(SUPPORT_TGLLP)
    set(HW_SOURCES_GEN12LP
        ${HW_SOURCES_GEN12LP}
        ${CMAKE_CURRENT_SOURCE_DIR}/CMakeLists.txt
        ${CMAKE_CURRENT_SOURCE_DIR}/cmdlist_tgllp.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/cmdqueue_tgllp.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/kernel_tgllp.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/image_tgllp.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/sampler_tgllp.cpp
        PARENT_SCOPE
    )
endif()
