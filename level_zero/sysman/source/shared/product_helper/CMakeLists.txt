#
# Copyright (C) 2025 Intel Corporation
#
# SPDX-License-Identifier: MIT
#

target_sources(${L0_STATIC_LIB_NAME}
               PRIVATE
               ${CMAKE_CURRENT_SOURCE_DIR}/sysman_os_agnostic_product_helper_hw.inl
               ${CMAKE_CURRENT_SOURCE_DIR}/sysman_os_agnostic_product_helper_xe2_and_later.inl
)
