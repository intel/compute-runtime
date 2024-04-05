#
# Copyright (C) 2021-2024 Intel Corporation
#
# SPDX-License-Identifier: MIT
#

target_sources(ze_intel_gpu_aub_tests PRIVATE
               ${CMAKE_CURRENT_SOURCE_DIR}/CMakeLists.txt
               ${CMAKE_CURRENT_SOURCE_DIR}/aub_fixture.cpp
               ${CMAKE_CURRENT_SOURCE_DIR}/aub_fixture.h
               ${CMAKE_CURRENT_SOURCE_DIR}/create_aub_ult_builtin_functions_lib.cpp
               ${CMAKE_CURRENT_SOURCE_DIR}/multicontext_l0_aub_fixture.h
               ${CMAKE_CURRENT_SOURCE_DIR}/multicontext_l0_aub_fixture.cpp

)
add_subdirectories()
