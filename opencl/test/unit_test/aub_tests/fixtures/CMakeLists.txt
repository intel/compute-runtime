#
# Copyright (C) 2018-2024 Intel Corporation
#
# SPDX-License-Identifier: MIT
#

target_sources(igdrcl_aub_tests PRIVATE
               ${CMAKE_CURRENT_SOURCE_DIR}/CMakeLists.txt
               ${CMAKE_CURRENT_SOURCE_DIR}/aub_fixture.cpp
               ${CMAKE_CURRENT_SOURCE_DIR}/aub_fixture.h
               ${CMAKE_CURRENT_SOURCE_DIR}/aub_walker_partition_fixture.cpp
               ${CMAKE_CURRENT_SOURCE_DIR}/aub_walker_partition_fixture.h
               ${CMAKE_CURRENT_SOURCE_DIR}/hello_world_fixture.h
               ${CMAKE_CURRENT_SOURCE_DIR}/image_aub_fixture.h
               ${CMAKE_CURRENT_SOURCE_DIR}/multicontext_ocl_aub_fixture.cpp
               ${CMAKE_CURRENT_SOURCE_DIR}/multicontext_ocl_aub_fixture.h
               ${CMAKE_CURRENT_SOURCE_DIR}/run_kernel_fixture.h
               ${CMAKE_CURRENT_SOURCE_DIR}/simple_arg_fixture.h
               ${CMAKE_CURRENT_SOURCE_DIR}/unified_memory_fixture.cpp
               ${CMAKE_CURRENT_SOURCE_DIR}/unified_memory_fixture.h
)
add_subdirectories()
