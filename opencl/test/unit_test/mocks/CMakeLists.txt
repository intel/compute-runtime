#
# Copyright (C) 2018-2025 Intel Corporation
#
# SPDX-License-Identifier: MIT
#

set(IGDRCL_SRCS_tests_mocks
    ${CMAKE_CURRENT_SOURCE_DIR}/CMakeLists.txt
    ${CMAKE_CURRENT_SOURCE_DIR}/mock_async_event_handler.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/mock_async_event_handler.h
    ${CMAKE_CURRENT_SOURCE_DIR}/mock_buffer.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/mock_buffer.h
    ${CMAKE_CURRENT_SOURCE_DIR}/mock_cl_device.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/mock_cl_device.h
    ${CMAKE_CURRENT_SOURCE_DIR}/mock_cl_execution_environment.h
    ${CMAKE_CURRENT_SOURCE_DIR}/mock_command_queue.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/mock_command_queue.h
    ${CMAKE_CURRENT_SOURCE_DIR}/mock_context.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/mock_context.h
    ${CMAKE_CURRENT_SOURCE_DIR}/mock_debug_program.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/mock_debug_program.h
    ${CMAKE_CURRENT_SOURCE_DIR}/mock_event.h
    ${CMAKE_CURRENT_SOURCE_DIR}/mock_gmm_resource_info_ocl.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/mock_image.h
    ${CMAKE_CURRENT_SOURCE_DIR}/mock_kernel.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/mock_kernel.h
    ${CMAKE_CURRENT_SOURCE_DIR}/mock_platform.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/mock_platform.h
    ${CMAKE_CURRENT_SOURCE_DIR}/mock_printf_handler.h
    ${CMAKE_CURRENT_SOURCE_DIR}/mock_program.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/mock_program.h
    ${CMAKE_CURRENT_SOURCE_DIR}/mock_sampler.h
    ${CMAKE_CURRENT_SOURCE_DIR}/mock_sharing_factory.h
    ${CMAKE_CURRENT_SOURCE_DIR}/mock_sharing_handler.h
    ${CMAKE_CURRENT_SOURCE_DIR}/ult_cl_device_factory.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/ult_cl_device_factory.h
    ${CMAKE_CURRENT_SOURCE_DIR}/ult_cl_device_factory_with_platform.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/ult_cl_device_factory_with_platform.h
    ${NEO_SHARED_DIRECTORY}/gmm_helper/page_table_mngr_impl.cpp
    ${NEO_SHARED_TEST_DIRECTORY}/common/mocks/mock_gmm_client_context.cpp
    ${NEO_SHARED_TEST_DIRECTORY}/common/mocks/mock_gmm_client_context.h
    ${CMAKE_CURRENT_SOURCE_DIR}/mock_builder.h
    ${CMAKE_CURRENT_SOURCE_DIR}/mock_builder.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/mock_mdi.h
    ${CMAKE_CURRENT_SOURCE_DIR}/mock_mdi.cpp
    ${NEO_CORE_tests_compiler_mocks}
)

if(WIN32)
  file(GLOB IGDRCL_SRC_tests_mock_wddm "${CMAKE_CURRENT_SOURCE_DIR}/mock_wddm2[0-9]\.*")
  list(APPEND IGDRCL_SRCS_tests_mocks
       ${CMAKE_CURRENT_SOURCE_DIR}/mock_d3d_objects.h
       ${IGDRCL_SRC_tests_mock_wddm}
  )
else()
  list(APPEND IGDRCL_SRCS_tests_mocks
       ${NEO_SHARED_DIRECTORY}/os_interface/linux/page_table_manager_functions.cpp
  )
endif()

add_library(igdrcl_mocks STATIC EXCLUDE_FROM_ALL ${IGDRCL_SRCS_tests_mocks})
add_subdirectories()

target_include_directories(igdrcl_mocks PRIVATE
                           $<TARGET_PROPERTY:gmock-gtest,INTERFACE_INCLUDE_DIRECTORIES>
                           $<TARGET_PROPERTY:${NEO_STATIC_LIB_NAME},INTERFACE_INCLUDE_DIRECTORIES>
)
target_compile_definitions(igdrcl_mocks PRIVATE MOCKABLE_VIRTUAL=virtual $<TARGET_PROPERTY:${NEO_STATIC_LIB_NAME},INTERFACE_COMPILE_DEFINITIONS>)

set_target_properties(igdrcl_mocks PROPERTIES POSITION_INDEPENDENT_CODE ON)
set_target_properties(igdrcl_mocks PROPERTIES FOLDER "test mocks")
create_project_source_tree(igdrcl_mocks)
