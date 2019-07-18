#
# Copyright (C) 2018-2019 Intel Corporation
#
# SPDX-License-Identifier: MIT
#

set(RUNTIME_SRCS_SAMPLER
  ${CMAKE_CURRENT_SOURCE_DIR}/CMakeLists.txt
  ${CMAKE_CURRENT_SOURCE_DIR}/sampler.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/sampler.h
  ${CMAKE_CURRENT_SOURCE_DIR}/sampler.inl
  ${CMAKE_CURRENT_SOURCE_DIR}/sampler_factory_init.inl
)
add_subdirectories()
target_sources(${NEO_STATIC_LIB_NAME} PRIVATE ${RUNTIME_SRCS_SAMPLER})
set_property(GLOBAL PROPERTY RUNTIME_SRCS_SAMPLER ${RUNTIME_SRCS_SAMPLER})
