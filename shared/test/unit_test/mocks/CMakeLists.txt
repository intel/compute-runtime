#
# Copyright (C) 2021-2022 Intel Corporation
#
# SPDX-License-Identifier: MIT
#

target_sources(neo_shared_tests PRIVATE
               ${CMAKE_CURRENT_SOURCE_DIR}/CMakeLists.txt
               ${CMAKE_CURRENT_SOURCE_DIR}/mock_dispatch_kernel_encoder_interface.h
               ${CMAKE_CURRENT_SOURCE_DIR}/mock_gmm_resource_info.cpp
)
