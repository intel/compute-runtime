#
# Copyright (C) 2020 Intel Corporation
#
# SPDX-License-Identifier: MIT
#

set(NEO_CORE_ENCODERS_TESTS
  ${CMAKE_CURRENT_SOURCE_DIR}/CMakeLists.txt
  ${CMAKE_CURRENT_SOURCE_DIR}/test_encode_atomic.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/test_encode_command_buffer.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/test_encode_dispatch_kernel.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/test_encode_math.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/test_encode_media_interface_descriptor.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/test_encode_semaphore.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/test_encode_set_mmio.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/test_encode_states.cpp
)
add_subdirectories()
set_property(GLOBAL PROPERTY NEO_CORE_ENCODERS_TESTS ${NEO_CORE_ENCODERS_TESTS})
