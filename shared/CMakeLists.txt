#
# Copyright (C) 2021-2025 Intel Corporation
#
# SPDX-License-Identifier: MIT
#

set(KERNEL_TYPES "-Didx_t=uint -Dcoord2_t=uint2 -Dcoord4_t=uint4 -Doffset_t=uint")
set(WIDE_STATELESS_TYPES "-Didx_t=ulong -Dcoord2_t=ulong2 -Dcoord4_t=ulong4 -Doffset_t=ulong")

add_subdirectory(source)
add_subdirectory(generate_cpp_array)

set(SHARED_TEST_PROJECTS_FOLDER "neo shared")

if(NOT NEO_SKIP_UNIT_TESTS)
  add_custom_target(unit_tests)
  add_subdirectory(test/common "${NEO_BUILD_DIR}/shared/test/common")
  if(NOT NEO_SKIP_SHARED_UNIT_TESTS)
    add_subdirectory(test/unit_test)
  endif()
endif()

set(TOOLS_DIR "${CMAKE_CURRENT_SOURCE_DIR}/tools${BRANCH_DIR_SUFFIX}")
if(EXISTS ${TOOLS_DIR} AND EXISTS ${TOOLS_DIR}/CMakeLists.txt)
  add_subdirectory(${TOOLS_DIR})
endif()
