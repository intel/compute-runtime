# Copyright (c) 2017 - 2018, Intel Corporation
#
# Permission is hereby granted, free of charge, to any person obtaining a
# copy of this software and associated documentation files (the "Software"),
# to deal in the Software without restriction, including without limitation
# the rights to use, copy, modify, merge, publish, distribute, sublicense,
# and/or sell copies of the Software, and to permit persons to whom the
# Software is furnished to do so, subject to the following conditions:
#
# The above copyright notice and this permission notice shall be included
# in all copies or substantial portions of the Software.
#
# THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS
# OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
# FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL
# THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR
# OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE,
# ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR
# OTHER DEALINGS IN THE SOFTWARE.

set(IGDRCL_SRCS_tests_helpers
  ${CMAKE_CURRENT_SOURCE_DIR}/CMakeLists.txt
  ${CMAKE_CURRENT_SOURCE_DIR}/array_count_tests.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/aligned_memory_tests.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/base_object_tests.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/base_object_tests_mt.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/basic_math_tests.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/cl_helper_tests.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/debug_helpers_tests.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/debug_manager_state_restore.h
  ${CMAKE_CURRENT_SOURCE_DIR}/dirty_state_helpers_tests.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/dispatch_info_tests.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/dispatch_info_builder_tests.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/file_io_tests.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/flush_stamp_tests.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/get_info_tests.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/gtest_helpers.h
  ${CMAKE_CURRENT_SOURCE_DIR}/hw_helper_default_tests.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/hw_helper_tests.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/hw_helper_tests.h
  ${CMAKE_CURRENT_SOURCE_DIR}/hw_parse.h
  ${CMAKE_CURRENT_SOURCE_DIR}/hw_parse.inl
  ${CMAKE_CURRENT_SOURCE_DIR}/kernel_commands_tests.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/kernel_filename_helper.h
  ${CMAKE_CURRENT_SOURCE_DIR}/kmd_notify_tests.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/l3_helper.h
  ${CMAKE_CURRENT_SOURCE_DIR}/l3_helper.inl
  ${CMAKE_CURRENT_SOURCE_DIR}/memory_management_tests.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/mipmap_tests.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/per_thread_data_tests.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/ptr_math_tests.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/queue_helpers_tests.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/sampler_helpers_tests.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/string_to_hash_tests.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/string_tests.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/task_information_tests.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/TestDebugVariables.inl
  ${CMAKE_CURRENT_SOURCE_DIR}/uint16_sse4_tests.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/validator_tests.cpp
)

set(IGDRCL_SRCS_tests_helpers_windows
  ${CMAKE_CURRENT_SOURCE_DIR}/windows/kmd_notify_windows_tests.cpp
)

set(IGDRCL_SRCS_tests_helpers_linux
  ${CMAKE_CURRENT_SOURCE_DIR}/linux/kmd_notify_linux_tests.cpp
)

if(WIN32)
  target_sources(igdrcl_tests PRIVATE ${IGDRCL_SRCS_tests_helpers_windows})
 else()
  target_sources(igdrcl_tests PRIVATE ${IGDRCL_SRCS_tests_helpers_linux})
 endif()

target_sources(igdrcl_tests PRIVATE ${IGDRCL_SRCS_tests_helpers})
set_property(GLOBAL PROPERTY IGDRCL_SRCS_tests_helpers_linux ${IGDRCL_SRCS_tests_helpers_linux})
