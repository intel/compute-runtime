#
# Copyright (C) 2019-2020 Intel Corporation
#
# SPDX-License-Identifier: MIT
#

if(SUPPORT_CFL)
    set(HW_SOURCES_GEN9
        ${HW_SOURCES_GEN9}
        ${CMAKE_CURRENT_SOURCE_DIR}/CMakeLists.txt
        ${CMAKE_CURRENT_SOURCE_DIR}/cmdlist_cfl.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/cmdqueue_cfl.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/kernel_cfl.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/image_cfl.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/sampler_cfl.cpp
        PARENT_SCOPE
    )
endif()
