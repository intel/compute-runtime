#
# Copyright (C) 2019-2024 Intel Corporation
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
    ${CMAKE_CURRENT_SOURCE_DIR}/compiler_options.h
    ${CMAKE_CURRENT_SOURCE_DIR}/compiler_options.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/compiler_options_extra.h
    ${CMAKE_CURRENT_SOURCE_DIR}${BRANCH_DIR_SUFFIX}compiler_options_extra.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/compiler_warnings/compiler_warnings.h
    ${CMAKE_CURRENT_SOURCE_DIR}/create_main.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/default_cache_config.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/default_cache_config.h
    ${CMAKE_CURRENT_SOURCE_DIR}/external_functions.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/external_functions.h
    ${CMAKE_CURRENT_SOURCE_DIR}/igc_platform_helper.h
    ${CMAKE_CURRENT_SOURCE_DIR}/igc_platform_helper.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/intermediate_representations.h
    ${CMAKE_CURRENT_SOURCE_DIR}/linker.h
    ${CMAKE_CURRENT_SOURCE_DIR}/linker.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/os_compiler_cache_helper.h
    ${CMAKE_CURRENT_SOURCE_DIR}/oclc_extensions.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}${BRANCH_DIR_SUFFIX}oclc_extensions_extra.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/tokenized_string.h
)

set_property(GLOBAL PROPERTY NEO_CORE_COMPILER_INTERFACE ${NEO_CORE_COMPILER_INTERFACE})
add_subdirectories()