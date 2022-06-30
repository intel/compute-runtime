#
# Copyright (C) 2020 Intel Corporation
#
# SPDX-License-Identifier: MIT
#

set(L0_SRCS_TOOLS_GLOBAL_OPERATIONS
    ${CMAKE_CURRENT_SOURCE_DIR}/global_operations.h
    ${CMAKE_CURRENT_SOURCE_DIR}/global_operations_imp.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/global_operations_imp.h
    ${CMAKE_CURRENT_SOURCE_DIR}/os_global_operations.h
)

target_include_directories(${L0_STATIC_LIB_NAME}
                           PUBLIC
                           ${CMAKE_CURRENT_SOURCE_DIR}/
)

target_sources(${L0_STATIC_LIB_NAME}
               PRIVATE
               ${L0_SRCS_TOOLS_GLOBAL_OPERATIONS}
               ${CMAKE_CURRENT_SOURCE_DIR}/CMakeLists.txt
)

if(UNIX)
  add_subdirectory(linux)
else()
  add_subdirectory(windows)
endif()

# Make our source files visible to parent
set_property(GLOBAL PROPERTY L0_SRCS_TOOLS_GLOBAL_OPERATIONS ${L0_SRCS_TOOLS_GLOBAL_OPERATIONS})
