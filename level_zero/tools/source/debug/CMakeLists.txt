#
# Copyright (C) 2021-2024 Intel Corporation
#
# SPDX-License-Identifier: MIT
#

target_sources(${L0_STATIC_LIB_NAME}
               PRIVATE
               ${CMAKE_CURRENT_SOURCE_DIR}/CMakeLists.txt
               ${CMAKE_CURRENT_SOURCE_DIR}/debug_session.h
               ${CMAKE_CURRENT_SOURCE_DIR}/debug_session_imp.cpp
               ${CMAKE_CURRENT_SOURCE_DIR}/debug_session_imp.h
               ${CMAKE_CURRENT_SOURCE_DIR}/debug_handlers.h
               ${CMAKE_CURRENT_SOURCE_DIR}/debug_handlers.cpp
               ${CMAKE_CURRENT_SOURCE_DIR}/eu_thread.h
               ${CMAKE_CURRENT_SOURCE_DIR}/CMakeLists.txt
               ${CMAKE_CURRENT_SOURCE_DIR}/${BRANCH_DIR_SUFFIX}thread_scratch_helper.cpp
)

add_subdirectories()
