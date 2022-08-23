#
# Copyright (C) 2021-2022 Intel Corporation
#
# SPDX-License-Identifier: MIT
#

if(TESTS_DG2)
  target_sources(${TARGET_NAME} PRIVATE
                 ${CMAKE_CURRENT_SOURCE_DIR}/CMakeLists.txt
                 ${CMAKE_CURRENT_SOURCE_DIR}/test_cache_flush_dg2.cpp
                 ${CMAKE_CURRENT_SOURCE_DIR}/test_cmdlist_dg2.cpp
                 ${CMAKE_CURRENT_SOURCE_DIR}/test_cmdqueue_dg2.cpp
                 ${CMAKE_CURRENT_SOURCE_DIR}/test_excludes_dg2.cpp
                 ${CMAKE_CURRENT_SOURCE_DIR}/test_kernel_dg2.cpp
                 ${CMAKE_CURRENT_SOURCE_DIR}/test_module_dg2.cpp
                 ${CMAKE_CURRENT_SOURCE_DIR}/test_sampler_dg2.cpp
  )
  add_subdirectories()
endif()
