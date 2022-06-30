#
# Copyright (C) 2018-2021 Intel Corporation
#
# SPDX-License-Identifier: MIT
#

set(RUNTIME_SRCS_COMPILER_INTERFACE
    ${CMAKE_CURRENT_SOURCE_DIR}/CMakeLists.txt
    ${CMAKE_CURRENT_SOURCE_DIR}/default_cl_cache_config.cpp
)

target_sources(${NEO_STATIC_LIB_NAME} PRIVATE
               ${RUNTIME_SRCS_COMPILER_INTERFACE}
               ${NEO_SHARED_DIRECTORY}/compiler_interface/oclc_extensions.cpp
               ${NEO_SHARED_DIRECTORY}/compiler_interface/oclc_extensions.h
)
set_property(GLOBAL PROPERTY RUNTIME_SRCS_COMPILER_INTERFACE ${RUNTIME_SRCS_COMPILER_INTERFACE})
