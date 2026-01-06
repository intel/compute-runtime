#
# Copyright (C) 2025 Intel Corporation
#
# SPDX-License-Identifier: MIT
#

target_sources(${TARGET_NAME}
               PRIVATE
               ${CMAKE_CURRENT_SOURCE_DIR}/CMakeLists.txt
               ${CMAKE_CURRENT_SOURCE_DIR}/mutable_cmdlist_fixture.cpp
               ${CMAKE_CURRENT_SOURCE_DIR}/mutable_cmdlist_fixture.h
               ${CMAKE_CURRENT_SOURCE_DIR}/mutable_hw_command_fixture.cpp
               ${CMAKE_CURRENT_SOURCE_DIR}/mutable_hw_command_fixture.h
               ${CMAKE_CURRENT_SOURCE_DIR}/mutable_indirect_data_fixture.cpp
               ${CMAKE_CURRENT_SOURCE_DIR}/mutable_indirect_data_fixture.h
               ${CMAKE_CURRENT_SOURCE_DIR}/variable_fixture.cpp
               ${CMAKE_CURRENT_SOURCE_DIR}/variable_fixture.h
)

add_subdirectories()
