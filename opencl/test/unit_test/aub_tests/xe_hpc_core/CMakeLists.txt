#
# Copyright (C) 2022 Intel Corporation
#
# SPDX-License-Identifier: MIT
#

if(TESTS_XE_HPC_CORE)
  target_sources(igdrcl_aub_tests PRIVATE
                 ${CMAKE_CURRENT_SOURCE_DIR}/CMakeLists.txt
                 ${CMAKE_CURRENT_SOURCE_DIR}/aub_mem_dump_tests_xe_hpc_core.cpp
                 ${CMAKE_CURRENT_SOURCE_DIR}/copy_engine_aub_tests_xe_hpc_core.cpp
                 ${CMAKE_CURRENT_SOURCE_DIR}/enqueue_kernel_aub_tests_xe_hpc_core.cpp
                 ${CMAKE_CURRENT_SOURCE_DIR}/system_memfence_aub_tests_xe_hpc_core.cpp
                 ${CMAKE_CURRENT_SOURCE_DIR}/um_stateless_compression_aub_tests_xe_hpc_core.cpp
                 ${CMAKE_CURRENT_SOURCE_DIR}/aub_test_excludes_xe_hpc_core.cpp
  )
  add_subdirectories()
endif()
