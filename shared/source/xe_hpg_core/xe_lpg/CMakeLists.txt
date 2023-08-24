#
# Copyright (C) 2023 Intel Corporation
#
# SPDX-License-Identifier: MIT
#

set_property(GLOBAL APPEND PROPERTY NEO_CORE_HELPERS
             ${CMAKE_CURRENT_SOURCE_DIR}/CMakeLists.txt
             ${CMAKE_CURRENT_SOURCE_DIR}/compiler_product_helper_xe_lpg.inl
             ${CMAKE_CURRENT_SOURCE_DIR}/device_ids_configs_xe_lpg.h
             ${CMAKE_CURRENT_SOURCE_DIR}/os_agnostic_product_helper_xe_lpg.inl
)
add_subdirectories()
