#
# Copyright (C) 2019-2020 Intel Corporation
#
# SPDX-License-Identifier: MIT
#

if(SUPPORT_ICLLP)
    set(HW_SOURCES_GEN11
        ${HW_SOURCES_GEN11}
        ${CMAKE_CURRENT_SOURCE_DIR}/CMakeLists.txt
        ${CMAKE_CURRENT_SOURCE_DIR}/cmdlist_icllp.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/cmdqueue_icllp.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/kernel_icllp.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/image_icllp.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/sampler_icllp.cpp
        PARENT_SCOPE
    )
endif()
