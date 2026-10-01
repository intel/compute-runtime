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

if(NOT MSVC)
  set_source_files_properties(${NEO_SHARED_TEST_DIRECTORY}/common/base_ult_config_listener.cpp
                              ${NEO_SHARED_TEST_DIRECTORY}/common/helpers/memory_management.cpp
                              ${NEO_SHARED_TEST_DIRECTORY}/common/test_macros/test_excludes.cpp
                              ${NEO_SHARED_TEST_DIRECTORY}/common/test_macros/test_matcher_registry.cpp
                              PROPERTIES COMPILE_OPTIONS "-O2;-fno-lto"
  )
endif()
