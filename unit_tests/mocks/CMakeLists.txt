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

set(IGDRCL_SRCS_tests_mocks
  ${CMAKE_CURRENT_SOURCE_DIR}/CMakeLists.txt
  ${CMAKE_CURRENT_SOURCE_DIR}/mock_32bitAllocator.h
  ${CMAKE_CURRENT_SOURCE_DIR}/mock_async_event_handler.h
  ${CMAKE_CURRENT_SOURCE_DIR}/mock_block_kernel_manager.h
  ${CMAKE_CURRENT_SOURCE_DIR}/mock_buffer.h
  ${CMAKE_CURRENT_SOURCE_DIR}/mock_builtins.h
  ${CMAKE_CURRENT_SOURCE_DIR}/mock_cif.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/mock_cif.h
  ${CMAKE_CURRENT_SOURCE_DIR}/mock_command_queue.h
  ${CMAKE_CURRENT_SOURCE_DIR}/mock_compilers.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/mock_compilers.h
  ${CMAKE_CURRENT_SOURCE_DIR}/mock_context.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/mock_context.h
  ${CMAKE_CURRENT_SOURCE_DIR}/mock_csr.h
  ${CMAKE_CURRENT_SOURCE_DIR}/mock_csr.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/mock_deferrable_deletion.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/mock_deferrable_deletion.h
  ${CMAKE_CURRENT_SOURCE_DIR}/mock_deferred_deleter.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/mock_deferred_deleter.h
  ${CMAKE_CURRENT_SOURCE_DIR}/mock_device_queue.h
  ${CMAKE_CURRENT_SOURCE_DIR}/mock_device.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/mock_device.h
  ${CMAKE_CURRENT_SOURCE_DIR}/mock_event.h
  ${CMAKE_CURRENT_SOURCE_DIR}/mock_gmm.h
  ${CMAKE_CURRENT_SOURCE_DIR}/mock_gmm_resource_info.h
  ${CMAKE_CURRENT_SOURCE_DIR}/mock_gmm_resource_info.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/mock_graphics_allocation.h
  ${CMAKE_CURRENT_SOURCE_DIR}${IGDRCL__INSTRUMENTATION_DIR_SUFFIX}/mock_instrumentation.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/mock_image.h
  ${CMAKE_CURRENT_SOURCE_DIR}/mock_kernel.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/mock_kernel.h
  ${CMAKE_CURRENT_SOURCE_DIR}/mock_memory_manager.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/mock_memory_manager.h
  ${CMAKE_CURRENT_SOURCE_DIR}/mock_ostime.h
  ${CMAKE_CURRENT_SOURCE_DIR}/mock_program.h
  ${CMAKE_CURRENT_SOURCE_DIR}/mock_program.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/mock_sampler.h
  ${CMAKE_CURRENT_SOURCE_DIR}/mock_sip.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/mock_sip.h
  ${CMAKE_CURRENT_SOURCE_DIR}/mock_source_level_debugger.h
  ${CMAKE_CURRENT_SOURCE_DIR}/mock_submissions_aggregator.h
)

if (WIN32)
  file(GLOB IGDRCL_SRC_tests_mock_wddm "${CMAKE_CURRENT_SOURCE_DIR}/mock_wddm2[0-9]\.*")
  list (APPEND IGDRCL_SRCS_tests_mocks
    ${CMAKE_CURRENT_SOURCE_DIR}/mock_d3d_objects.h
    ${CMAKE_CURRENT_SOURCE_DIR}/mock_ostime_win.h
    ${CMAKE_CURRENT_SOURCE_DIR}${BRANCH_DIR_SUFFIX}/mock_gmm_memory.h
    ${CMAKE_CURRENT_SOURCE_DIR}${BRANCH_DIR_SUFFIX}/mock_gmm_memory.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/mock_gmm_page_table_mngr.h
    ${CMAKE_CURRENT_SOURCE_DIR}/mock_gmm_page_table_mngr.cpp
    ${IGDRCL_SRC_tests_mock_wddm}
  )
endif()

add_subdirectories()

add_library(igdrcl_mocks STATIC ${IGDRCL_SRCS_tests_mocks})

# add_dependencies(igdrcl_mocks gmock-gtest)

target_include_directories(igdrcl_mocks PRIVATE 
    $<TARGET_PROPERTY:gmock-gtest,INTERFACE_INCLUDE_DIRECTORIES>
    $<TARGET_PROPERTY:${NEO_STATIC_LIB_NAME},INTERFACE_INCLUDE_DIRECTORIES>
)
target_compile_definitions(igdrcl_mocks PRIVATE MOCKABLE_VIRTUAL=virtual $<TARGET_PROPERTY:${NEO_STATIC_LIB_NAME},INTERFACE_COMPILE_DEFINITIONS>)

set_target_properties(igdrcl_mocks PROPERTIES POSITION_INDEPENDENT_CODE ON)
set_target_properties(igdrcl_mocks PROPERTIES CXX_STANDARD 11 CXX_STANDARD_REQUIRED ON)
set_target_properties(igdrcl_mocks PROPERTIES FOLDER "test mocks")
create_project_source_tree(igdrcl_mocks)
