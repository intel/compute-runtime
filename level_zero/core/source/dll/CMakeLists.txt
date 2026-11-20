#
# Copyright (C) 2020-2026 Intel Corporation
#
# SPDX-License-Identifier: MIT
#

target_sources(${TARGET_NAME_L0}
               PRIVATE
               ${CMAKE_CURRENT_SOURCE_DIR}/CMakeLists.txt
               ${NEO_SHARED_DIRECTORY}/helpers/allow_deferred_deleter.cpp
               ${CMAKE_CURRENT_SOURCE_DIR}/create_builtin_functions_lib.cpp
               ${CMAKE_CURRENT_SOURCE_DIR}/event_completion_timeout.cpp
               ${NEO_SHARED_DIRECTORY}/helpers/file_io.cpp
)

add_subdirectories()
