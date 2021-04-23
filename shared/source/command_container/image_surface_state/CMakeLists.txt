#
# Copyright (C) 2021 Intel Corporation
#
# SPDX-License-Identifier: MIT
#

set(NEO_CORE_COMMAND_CONTAINER_IMAGE_SURFACE_STATE
    ${CMAKE_CURRENT_SOURCE_DIR}/CMakeLists.txt
    ${CMAKE_CURRENT_SOURCE_DIR}/compression_params_bdw_plus.inl
    ${CMAKE_CURRENT_SOURCE_DIR}/compression_params_tgllp_plus.inl
    ${CMAKE_CURRENT_SOURCE_DIR}/compression_params_xehp_plus.inl
)

set_property(GLOBAL APPEND PROPERTY NEO_CORE_COMMAND_CONTAINER ${NEO_CORE_COMMAND_CONTAINER_IMAGE_SURFACE_STATE})

add_subdirectories()
