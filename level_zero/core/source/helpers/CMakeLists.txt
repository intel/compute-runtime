#
# Copyright (C) 2022-2025 Intel Corporation
#
# SPDX-License-Identifier: MIT
#

target_sources(${L0_STATIC_LIB_NAME}
               PRIVATE
               ${CMAKE_CURRENT_SOURCE_DIR}/CMakeLists.txt
               ${CMAKE_CURRENT_SOURCE_DIR}/api_handle_helper.h
               ${CMAKE_CURRENT_SOURCE_DIR}/api_specific_config_l0.cpp
               ${CMAKE_CURRENT_SOURCE_DIR}/default_descriptors.h
               ${CMAKE_CURRENT_SOURCE_DIR}/default_descriptors.cpp
               ${CMAKE_CURRENT_SOURCE_DIR}/error_code_helper_l0.cpp
               ${CMAKE_CURRENT_SOURCE_DIR}/error_code_helper_l0.h
               ${CMAKE_CURRENT_SOURCE_DIR}/implicit_scaling_l0.cpp
               ${CMAKE_CURRENT_SOURCE_DIR}/l0_gfx_core_helper_factory_init.inl
               ${CMAKE_CURRENT_SOURCE_DIR}/l0_populate_factory.h
               ${CMAKE_CURRENT_SOURCE_DIR}/properties_parser.h
)
