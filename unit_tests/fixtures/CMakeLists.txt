# Copyright (c) 2018, Intel Corporation
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

set(IGDRCL_SRCS_tests_fixtures
  ${CMAKE_CURRENT_SOURCE_DIR}/CMakeLists.txt
  ${CMAKE_CURRENT_SOURCE_DIR}/context_fixture.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/context_fixture.h
  ${CMAKE_CURRENT_SOURCE_DIR}/device_host_queue_fixture.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/device_host_queue_fixture.h
  ${CMAKE_CURRENT_SOURCE_DIR}/enqueue_handler_fixture.h
  ${CMAKE_CURRENT_SOURCE_DIR}/execution_model_fixture.h
  ${CMAKE_CURRENT_SOURCE_DIR}/execution_model_kernel_fixture.h
  ${CMAKE_CURRENT_SOURCE_DIR}/hello_world_fixture.h
  ${CMAKE_CURRENT_SOURCE_DIR}/hello_world_kernel_fixture.h
  ${CMAKE_CURRENT_SOURCE_DIR}/image_fixture.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/image_fixture.h
  ${CMAKE_CURRENT_SOURCE_DIR}/kernel_arg_fixture.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/kernel_arg_fixture.h
  ${CMAKE_CURRENT_SOURCE_DIR}/kernel_data_fixture.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/kernel_data_fixture.h
  ${CMAKE_CURRENT_SOURCE_DIR}/media_kernel_fixture.h
  ${CMAKE_CURRENT_SOURCE_DIR}/memory_allocator_fixture.h
  ${CMAKE_CURRENT_SOURCE_DIR}/memory_manager_fixture.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/memory_manager_fixture.h
  ${CMAKE_CURRENT_SOURCE_DIR}/platform_fixture.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/platform_fixture.h
  ${CMAKE_CURRENT_SOURCE_DIR}/preemption_fixture.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/preemption_fixture.h
  ${CMAKE_CURRENT_SOURCE_DIR}/run_kernel_fixture.h
  ${CMAKE_CURRENT_SOURCE_DIR}/scenario_test_fixture.h
  ${CMAKE_CURRENT_SOURCE_DIR}/simple_arg_fixture.h
  ${CMAKE_CURRENT_SOURCE_DIR}/simple_arg_kernel_fixture.h
  ${CMAKE_CURRENT_SOURCE_DIR}/two_walker_fixture.h
  ${CMAKE_CURRENT_SOURCE_DIR}/ult_command_stream_receiver_fixture.h
)
target_sources(igdrcl_tests PRIVATE ${IGDRCL_SRCS_tests_fixtures})
set_property(GLOBAL PROPERTY IGDRCL_SRCS_tests_fixtures ${IGDRCL_SRCS_tests_fixtures})