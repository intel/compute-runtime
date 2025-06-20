#
# Copyright (C) 2021-2025 Intel Corporation
#
# SPDX-License-Identifier: MIT
#

target_sources(neo_shared_tests PRIVATE
               ${CMAKE_CURRENT_SOURCE_DIR}/CMakeLists.txt
               ${CMAKE_CURRENT_SOURCE_DIR}/mock_cpuid_functions.cpp
               ${CMAKE_CURRENT_SOURCE_DIR}/mock_cpuid_functions.h
               ${CMAKE_CURRENT_SOURCE_DIR}/mock_csr_simulated_common_hw.h
               ${CMAKE_CURRENT_SOURCE_DIR}/mock_dispatch_kernel_encoder_interface.h
               ${CMAKE_CURRENT_SOURCE_DIR}/mock_gmm_resource_info.cpp
               ${CMAKE_CURRENT_SOURCE_DIR}/mock_lrca_helper.h
               ${CMAKE_CURRENT_SOURCE_DIR}/mock_physical_address_allocator.h
)

if(WIN32)
  target_sources(neo_shared_tests PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/windows/mock_wddm_direct_submission.h)
endif()