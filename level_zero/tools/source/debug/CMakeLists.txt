#
# Copyright (C) 2021 Intel Corporation
#
# SPDX-License-Identifier: MIT
#

set(L0_SRCS_TOOLS_DEBUG
    ${CMAKE_CURRENT_SOURCE_DIR}/debug_session.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/debug_session.h
    ${CMAKE_CURRENT_SOURCE_DIR}/debug_handlers.h
    ${CMAKE_CURRENT_SOURCE_DIR}/eu_thread.h
    ${CMAKE_CURRENT_SOURCE_DIR}${BRANCH_DIR_SUFFIX}debug_handlers.cpp
)

add_subdirectories()

target_sources(${L0_STATIC_LIB_NAME}
               PRIVATE
               ${L0_SRCS_TOOLS_DEBUG}
               ${CMAKE_CURRENT_SOURCE_DIR}/CMakeLists.txt
)
