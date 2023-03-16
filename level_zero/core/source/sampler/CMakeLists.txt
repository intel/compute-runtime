#
# Copyright (C) 2023 Intel Corporation
#
# SPDX-License-Identifier: MIT
#

target_sources(${L0_STATIC_LIB_NAME}
               PRIVATE
               ${CMAKE_CURRENT_SOURCE_DIR}/CMakeLists.txt
               ${CMAKE_CURRENT_SOURCE_DIR}/sampler.h
               ${CMAKE_CURRENT_SOURCE_DIR}/sampler_hw.h
               ${CMAKE_CURRENT_SOURCE_DIR}/sampler_hw.inl
               ${CMAKE_CURRENT_SOURCE_DIR}/sampler_imp.cpp
               ${CMAKE_CURRENT_SOURCE_DIR}/sampler_imp.h
)
