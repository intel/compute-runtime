#
# Copyright (C) 2021-2026 Intel Corporation
#
# SPDX-License-Identifier: MIT
#

set(KERNEL_TYPES "-Didx_t=uint -Dcoord2_t=uint2 -Dcoord4_t=uint4 -Doffset_t=uint")
set(WIDE_STATELESS_TYPES "-Didx_t=ulong -Dcoord2_t=ulong2 -Dcoord4_t=ulong4 -Doffset_t=ulong")
set(LSC_STORE_INTRINSICS "-DLSC_STCC_DEFAULT=0 -DLSC_STCC_L1UC_L3UC=1 -DLSC_STCC_L1UC_L3WB=2 -DLSC_STCC_L1WT_L3UC=3 -DLSC_STCC_L1WT_L3WB=4 -DLSC_STCC_L1S_L3UC=5 -DLSC_STCC_L1S_L3WB=6 -DLSC_STCC_L1WB_L3WB=7")

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
