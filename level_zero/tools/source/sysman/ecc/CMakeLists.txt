#
# Copyright (C) 2022 Intel Corporation
#
# SPDX-License-Identifier: MIT
#

set(L0_SRCS_TOOLS_SYSMAN_ECC
    ${CMAKE_CURRENT_SOURCE_DIR}/ecc.h
    ${CMAKE_CURRENT_SOURCE_DIR}/ecc_imp.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/ecc_imp.h
)

target_include_directories(${L0_STATIC_LIB_NAME}
                           PUBLIC
                           ${CMAKE_CURRENT_SOURCE_DIR}/
)

target_sources(${L0_STATIC_LIB_NAME}
               PRIVATE
               ${L0_SRCS_TOOLS_SYSMAN_ECC}
               ${CMAKE_CURRENT_SOURCE_DIR}/CMakeLists.txt
)

add_subdirectories()

# Make our source files visible to parent
set_property(GLOBAL PROPERTY L0_SRCS_TOOLS_ECC ${L0_SRCS_TOOLS_ECC})
