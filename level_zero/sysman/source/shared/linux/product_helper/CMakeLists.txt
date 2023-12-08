#
# Copyright (C) 2023 Intel Corporation
#
# SPDX-License-Identifier: MIT
#

target_sources(${L0_STATIC_LIB_NAME}
               PRIVATE
               ${CMAKE_CURRENT_SOURCE_DIR}/CMakeLists.txt
               ${CMAKE_CURRENT_SOURCE_DIR}/sysman_product_helper.h
               ${CMAKE_CURRENT_SOURCE_DIR}/sysman_product_helper_hw.h
               ${CMAKE_CURRENT_SOURCE_DIR}/sysman_product_helper_hw.inl
               ${CMAKE_CURRENT_SOURCE_DIR}/sysman_product_helper.cpp
               ${CMAKE_CURRENT_SOURCE_DIR}/sysman_product_helper_xe_hp_and_later.inl
)

add_subdirectories()

