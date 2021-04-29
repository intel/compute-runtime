#
# Copyright (C) 2021 Intel Corporation
#
# SPDX-License-Identifier: MIT
#

target_sources(ze_intel_gpu_aub_tests PRIVATE
               ${CMAKE_CURRENT_SOURCE_DIR}/CMakeLists.txt
               ${CMAKE_CURRENT_SOURCE_DIR}/aub_fixture.cpp
               ${CMAKE_CURRENT_SOURCE_DIR}/aub_fixture.h

)
add_subdirectories()
