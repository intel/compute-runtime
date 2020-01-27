#
# Copyright (C) 2019-2020 Intel Corporation
#
# SPDX-License-Identifier: MIT
#

set(NEO_CORE_COMMAND_STREAM
  ${CMAKE_CURRENT_SOURCE_DIR}/CMakeLists.txt
  ${CMAKE_CURRENT_SOURCE_DIR}/aub_subcapture_status.h
  ${CMAKE_CURRENT_SOURCE_DIR}/csr_definitions.h
  ${CMAKE_CURRENT_SOURCE_DIR}/csr_deps.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/csr_deps.h
  ${CMAKE_CURRENT_SOURCE_DIR}/linear_stream.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/linear_stream.h
  ${CMAKE_CURRENT_SOURCE_DIR}/preemption_mode.h
  ${CMAKE_CURRENT_SOURCE_DIR}/preemption.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/preemption.h
  ${CMAKE_CURRENT_SOURCE_DIR}/preemption.inl
  ${CMAKE_CURRENT_SOURCE_DIR}/scratch_space_controller.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/scratch_space_controller.h
  ${CMAKE_CURRENT_SOURCE_DIR}/scratch_space_controller_base.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/scratch_space_controller_base.h
  ${CMAKE_CURRENT_SOURCE_DIR}/submissions_aggregator.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/submissions_aggregator.h
  ${CMAKE_CURRENT_SOURCE_DIR}/thread_arbitration_policy.h
)

set_property(GLOBAL PROPERTY NEO_CORE_COMMAND_STREAM ${NEO_CORE_COMMAND_STREAM})
add_subdirectories()