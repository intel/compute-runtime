#
# Copyright (C) 2018-2019 Intel Corporation
#
# SPDX-License-Identifier: MIT
#

set(IGDRCL_SRCS_tests_fixtures
  ${CMAKE_CURRENT_SOURCE_DIR}/CMakeLists.txt
  ${CMAKE_CURRENT_SOURCE_DIR}/context_fixture.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/context_fixture.h
  ${CMAKE_CURRENT_SOURCE_DIR}/device_host_queue_fixture.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/device_host_queue_fixture.h
  ${CMAKE_CURRENT_SOURCE_DIR}/device_instrumentation_fixture.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/device_instrumentation_fixture.h
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
