#
# Copyright (C) 2018 Intel Corporation
#
# SPDX-License-Identifier: MIT
#

set(target_name mock_gmm)
project(${target_name})

# Setting up our local list of test files
set(IGDRCL_SRCS_tests_mock_gmm
  ${CMAKE_CURRENT_SOURCE_DIR}/CMakeLists.txt
  ${CMAKE_CURRENT_SOURCE_DIR}/mock_gmm.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/mock_gmm.def
)
add_library(${target_name} EXCLUDE_FROM_ALL SHARED ${IGDRCL_SRCS_tests_mock_gmm})

target_include_directories(${target_name} PRIVATE
  ${WDK_INCLUDE_PATHS}
  ${UMKM_SHAREDDATA_INCLUDE_PATHS}
)

set_target_properties(
  ${target_name}
  PROPERTIES DEBUG_OUTPUT_NAME ${target_name}
  RELEASE_OUTPUT_NAME ${target_name}
  RELEASEINTERNAL_OUTPUT_NAME ${target_name}
  OUTPUT_NAME ${target_name}
)

create_project_source_tree(${target_name})
set_target_properties(${target_name} PROPERTIES FOLDER "test mocks")
target_compile_definitions(${target_name} PUBLIC)
add_dependencies(unit_tests ${target_name})

