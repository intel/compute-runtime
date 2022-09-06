#
# Copyright (C) 2019-2022 Intel Corporation
#
# SPDX-License-Identifier: MIT
#

set(NEO_CORE_COMPILER_INTERFACE
    ${CMAKE_CURRENT_SOURCE_DIR}/CMakeLists.txt
    ${CMAKE_CURRENT_SOURCE_DIR}/compiler_cache.h
    ${CMAKE_CURRENT_SOURCE_DIR}/compiler_cache.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/compiler_interface.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/compiler_interface.h
    ${CMAKE_CURRENT_SOURCE_DIR}/compiler_interface.inl
    ${CMAKE_CURRENT_SOURCE_DIR}/create_main.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/default_cache_config.h
    ${CMAKE_CURRENT_SOURCE_DIR}/external_functions.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/external_functions.h
    ${CMAKE_CURRENT_SOURCE_DIR}/intermediate_representations.h
    ${CMAKE_CURRENT_SOURCE_DIR}/linker.h
    ${CMAKE_CURRENT_SOURCE_DIR}/linker.inl
    ${CMAKE_CURRENT_SOURCE_DIR}/linker.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/compiler_options.h
    ${CMAKE_CURRENT_SOURCE_DIR}/compiler_options.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/compiler_warnings/compiler_warnings.h
)

set_property(GLOBAL PROPERTY NEO_CORE_COMPILER_INTERFACE ${NEO_CORE_COMPILER_INTERFACE})
