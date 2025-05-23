#
# Copyright (C) 2018-2025 Intel Corporation
#
# SPDX-License-Identifier: MIT
#

set(IGDRCL_SRCS_tests_fixtures
    ${CMAKE_CURRENT_SOURCE_DIR}/CMakeLists.txt
    ${CMAKE_CURRENT_SOURCE_DIR}/buffer_enqueue_fixture.h
    ${CMAKE_CURRENT_SOURCE_DIR}/cl_preemption_fixture.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/cl_preemption_fixture.h
    ${CMAKE_CURRENT_SOURCE_DIR}/d3d_test_fixture.h
    ${CMAKE_CURRENT_SOURCE_DIR}/dispatch_flags_fixture.h
    ${CMAKE_CURRENT_SOURCE_DIR}/device_info_fixture.h
    ${CMAKE_CURRENT_SOURCE_DIR}/device_instrumentation_fixture.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/device_instrumentation_fixture.h
    ${CMAKE_CURRENT_SOURCE_DIR}/enqueue_handler_fixture.h
    ${CMAKE_CURRENT_SOURCE_DIR}/hello_world_fixture.h
    ${CMAKE_CURRENT_SOURCE_DIR}/hello_world_kernel_fixture.h
    ${CMAKE_CURRENT_SOURCE_DIR}/image_fixture.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/image_fixture.h
    ${CMAKE_CURRENT_SOURCE_DIR}/kernel_arg_fixture.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/kernel_arg_fixture.h
    ${CMAKE_CURRENT_SOURCE_DIR}/kernel_data_fixture.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/kernel_data_fixture.h
    ${CMAKE_CURRENT_SOURCE_DIR}/kernel_work_group_info_fixture.h
    ${CMAKE_CURRENT_SOURCE_DIR}/multi_root_device_fixture.h
    ${CMAKE_CURRENT_SOURCE_DIR}/multi_tile_fixture.h
    ${CMAKE_CURRENT_SOURCE_DIR}/one_mip_level_image_fixture.h
    ${CMAKE_CURRENT_SOURCE_DIR}/platform_fixture.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/platform_fixture.h
    ${CMAKE_CURRENT_SOURCE_DIR}/run_kernel_fixture.h
    ${CMAKE_CURRENT_SOURCE_DIR}/scenario_test_fixture.h
    ${CMAKE_CURRENT_SOURCE_DIR}/simple_arg_fixture.h
    ${CMAKE_CURRENT_SOURCE_DIR}/simple_arg_kernel_fixture.h
    ${CMAKE_CURRENT_SOURCE_DIR}/two_walker_fixture.h
    ${CMAKE_CURRENT_SOURCE_DIR}/ult_command_stream_receiver_fixture.h
)

target_sources(igdrcl_tests PRIVATE ${IGDRCL_SRCS_tests_fixtures})
set_property(GLOBAL PROPERTY IGDRCL_SRCS_tests_fixtures ${IGDRCL_SRCS_tests_fixtures})
add_subdirectories()
