#
# Copyright (C) 2026 Intel Corporation
#
# SPDX-License-Identifier: MIT
#

target_sources(${TARGET_NAME_L0}
               PRIVATE
               ${CMAKE_CURRENT_SOURCE_DIR}/CMakeLists.txt
               ${CMAKE_CURRENT_SOURCE_DIR}/find_l0_loader.cmake
               ${CMAKE_CURRENT_SOURCE_DIR}/FindLevelZero.cmake
               ${CMAKE_CURRENT_SOURCE_DIR}/source_tree.cmake
               ${CMAKE_CURRENT_SOURCE_DIR}/UdevRulesDir.cmake
               ${CMAKE_CURRENT_SOURCE_DIR}/verify_headers.cmake
)

add_subdirectories()
