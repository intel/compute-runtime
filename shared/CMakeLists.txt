#
# Copyright (C) 2021 Intel Corporation
#
# SPDX-License-Identifier: MIT
#

add_subdirectory(source)
add_subdirectory(generate_cpp_array)
add_subdirectory(test/unit_test)
set(TOOLS_DIR "${CMAKE_CURRENT_SOURCE_DIR}/tools${BRANCH_DIR_SUFFIX}")
if(EXISTS ${TOOLS_DIR} AND EXISTS ${TOOLS_DIR}/CMakeLists.txt)
  add_subdirectory(${TOOLS_DIR})
endif()

