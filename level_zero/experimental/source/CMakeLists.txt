#
# Copyright (C) 2019-2020 Intel Corporation
#
# SPDX-License-Identifier: MIT
#

set(L0_EXPERIMENTAL_SOURCES
	${CMAKE_CURRENT_SOURCE_DIR}/experimental.cpp
)

target_include_directories(${TARGET_NAME_L0}
    PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/
)
target_sources(${TARGET_NAME_L0}
    PRIVATE
        ${L0_EXPERIMENTAL_SOURCES}
        ${CMAKE_CURRENT_SOURCE_DIR}/CMakeLists.txt
)

# Make our source files visible to parent
set_property(GLOBAL PROPERTY L0_EXPERIMENTAL_SOURCES ${L0_EXPERIMENTAL_SOURCES})
