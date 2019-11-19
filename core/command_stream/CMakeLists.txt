#
# Copyright (C) 2019 Intel Corporation
#
# SPDX-License-Identifier: MIT
#

set(NEO_CORE_COMMAND_STREAM
  ${CMAKE_CURRENT_SOURCE_DIR}/CMakeLists.txt
  ${CMAKE_CURRENT_SOURCE_DIR}/linear_stream.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/linear_stream.h
  ${CMAKE_CURRENT_SOURCE_DIR}/preemption_mode.h
  ${CMAKE_CURRENT_SOURCE_DIR}/preemption.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/preemption.h
  ${CMAKE_CURRENT_SOURCE_DIR}/preemption.inl
)

set_property(GLOBAL PROPERTY NEO_CORE_COMMAND_STREAM ${NEO_CORE_COMMAND_STREAM})
add_subdirectories()