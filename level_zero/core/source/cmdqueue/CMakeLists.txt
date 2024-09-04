#
# Copyright (C) 2020-2024 Intel Corporation
#
# SPDX-License-Identifier: MIT
#

target_sources(${L0_STATIC_LIB_NAME}
               PRIVATE
               ${CMAKE_CURRENT_SOURCE_DIR}/CMakeLists.txt
               ${CMAKE_CURRENT_SOURCE_DIR}/cmdqueue.cpp
               ${CMAKE_CURRENT_SOURCE_DIR}/cmdqueue.h
               ${CMAKE_CURRENT_SOURCE_DIR}/cmdqueue_hw.h
               ${CMAKE_CURRENT_SOURCE_DIR}/cmdqueue_hw.inl
               ${CMAKE_CURRENT_SOURCE_DIR}/cmdqueue_hw_skl_to_tgllp.inl
               ${CMAKE_CURRENT_SOURCE_DIR}/cmdqueue_imp.h
)
if(SUPPORT_XEHP_AND_LATER)
  target_sources(${L0_STATIC_LIB_NAME}
                 PRIVATE
                 ${CMAKE_CURRENT_SOURCE_DIR}/cmdqueue_xe_hp_core_and_later.inl
  )
endif()

add_subdirectories()
