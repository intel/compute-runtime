#
# Copyright (C) 2020-2022 Intel Corporation
#
# SPDX-License-Identifier: MIT
#

target_sources(${TARGET_NAME}
               PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/CMakeLists.txt
               $<TARGET_OBJECTS:${TARGET_NAME_L0}_fixtures>
)

add_subdirectories()
