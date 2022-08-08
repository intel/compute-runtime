#
# Copyright (C) 2020-2022 Intel Corporation
#
# SPDX-License-Identifier: MIT
#

add_subdirectory(debug)
add_subdirectory(metrics)
add_subdirectory(sysman)
add_subdirectory(pin)

target_sources(${L0_STATIC_LIB_NAME}
               PRIVATE
               ${L0_TOOLS_SOURCES}
               ${CMAKE_CURRENT_SOURCE_DIR}/CMakeLists.txt
)

# Make our source files visible to parent
set_property(GLOBAL PROPERTY L0_TOOLS_SOURCES ${L0_TOOLS_SOURCES})
