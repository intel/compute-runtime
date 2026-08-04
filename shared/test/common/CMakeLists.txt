#
# Copyright (C) 2021-2026 Intel Corporation
#
# SPDX-License-Identifier: MIT
#

include(${NEO_SOURCE_DIR}/cmake/setup_ult_global_flags.cmake)

set(NEO_LIBULT_COMMON_SRCS
    ${CMAKE_CURRENT_SOURCE_DIR}/CMakeLists.txt
    ${CMAKE_CURRENT_SOURCE_DIR}/tests_configuration.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/tests_configuration.h
)
if(USE_ASAN)
  list(APPEND NEO_LIBULT_COMMON_SRCS
       ${CMAKE_CURRENT_SOURCE_DIR}/default_asan_options.cpp
  )
endif()
add_library(neo_libult_common OBJECT EXCLUDE_FROM_ALL ${NEO_LIBULT_COMMON_SRCS})

add_subdirectories()

add_custom_target(prepare_test_kernels_for_shared)
add_dependencies(prepare_test_kernels_for_shared ${BUILTINS_BINARIES_LIB_NAME})
if(TARGET ${BUILTINS_SPIRV_LIB_NAME})
  add_dependencies(prepare_test_kernels_for_shared ${BUILTINS_SPIRV_LIB_NAME})
endif()
set_target_properties(prepare_test_kernels_for_shared PROPERTIES FOLDER "${SHARED_TEST_PROJECTS_FOLDER}/prepare test files")

add_dependencies(prepare_kernels ${BUILTINS_BINARIES_LIB_NAME})
if(TARGET ${BUILTINS_SPIRV_LIB_NAME})
  add_dependencies(prepare_kernels ${BUILTINS_SPIRV_LIB_NAME})
endif()
