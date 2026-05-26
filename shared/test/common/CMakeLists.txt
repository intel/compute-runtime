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
