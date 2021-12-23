#
# Copyright (C) 2018-2021 Intel Corporation
#
# SPDX-License-Identifier: MIT
#

macro(macro_for_each_core_type)
  foreach(BRANCH_DIR ${BRANCH_DIR_LIST})
    if(EXISTS ${NEO_SOURCE_DIR}/opencl/source${BRANCH_DIR}${CORE_TYPE_LOWER}/enable_family_full_ocl_${CORE_TYPE_LOWER}.cpp)
      list(APPEND IGDRCL_SRCS_ENABLE_TESTED_HW
           ${NEO_SOURCE_DIR}/opencl/source${BRANCH_DIR}${CORE_TYPE_LOWER}/enable_family_full_ocl_${CORE_TYPE_LOWER}.cpp
      )
    endif()
  endforeach()
endmacro()

apply_macro_for_each_core_type("TESTED")

set(IGDRCL_SRCS_LIB_ULT
    ${NEO_SOURCE_DIR}/opencl/source/compiler_interface/default_cache_config.cpp
    ${NEO_SOURCE_DIR}/opencl/source/dll/debug_manager.cpp
    ${NEO_SOURCE_DIR}/opencl/source/helpers/api_specific_config_ocl.cpp
    ${NEO_SOURCE_DIR}/opencl/source/helpers/implicit_scaling_ocl.cpp
    ${NEO_SOURCE_DIR}/opencl/source/memory_manager/compression_selector_ocl.cpp
    ${NEO_SOURCE_DIR}/opencl/source/memory_manager/cpu_page_fault_manager_memory_sync.cpp
    ${NEO_SOURCE_DIR}/shared/source/helpers/allow_deferred_deleter.cpp
    ${NEO_SOURCE_DIR}/opencl/test/unit_test/helpers/cl_execution_environment_helper.cpp
    ${NEO_SOURCE_DIR}/opencl/test/unit_test/helpers/cl_execution_environment_helper.h
)

add_library(igdrcl_libult OBJECT EXCLUDE_FROM_ALL
            ${IGDRCL_SRCS_LIB_ULT}
            ${IGDRCL_SRCS_ENABLE_TESTED_HW}
)

set(IGDRCL_SRCS_LIB_ULT_ENV
    ${CMAKE_CURRENT_SOURCE_DIR}/command_queue_ult.cpp
    ${NEO_SOURCE_DIR}/shared/test/common/helpers/custom_event_listener.h
    ${NEO_SOURCE_DIR}/opencl/test/unit_test/main.cpp
    ${NEO_SOURCE_DIR}/opencl/test/unit_test/command_queue/command_queue_fixture.cpp
    ${NEO_SOURCE_DIR}/opencl/test/unit_test/command_queue/command_queue_fixture.h
    ${NEO_SOURCE_DIR}/opencl/test/unit_test/fixtures/built_in_fixture.cpp
    ${NEO_SOURCE_DIR}/opencl/test/unit_test/fixtures/built_in_fixture.h
    ${NEO_SOURCE_DIR}/opencl/test/unit_test/fixtures/buffer_fixture.cpp
    ${NEO_SOURCE_DIR}/opencl/test/unit_test/fixtures/buffer_fixture.h
    ${NEO_SOURCE_DIR}/opencl/test/unit_test/fixtures/cl_device_fixture.cpp
    ${NEO_SOURCE_DIR}/opencl/test/unit_test/fixtures/cl_device_fixture.h
    ${NEO_SOURCE_DIR}/opencl/test/unit_test/fixtures/program_fixture.cpp
    ${NEO_SOURCE_DIR}/opencl/test/unit_test/fixtures/program_fixture.h
    ${NEO_SOURCE_DIR}/opencl/test/unit_test/indirect_heap/indirect_heap_fixture.cpp
    ${NEO_SOURCE_DIR}/opencl/test/unit_test/indirect_heap/indirect_heap_fixture.h
    ${NEO_SOURCE_DIR}/opencl/test/unit_test/ult_config_listener.cpp
    ${NEO_SOURCE_DIR}/opencl/test/unit_test/ult_config_listener.h
)

add_library(igdrcl_libult_env OBJECT EXCLUDE_FROM_ALL
            ${IGDRCL_SRCS_LIB_ULT_ENV}
)

target_include_directories(igdrcl_libult_env PRIVATE
                           $<TARGET_PROPERTY:gmock-gtest,INTERFACE_INCLUDE_DIRECTORIES>
)

set_property(GLOBAL PROPERTY IGDRCL_SRCS_ENABLE_TESTED_HW ${IGDRCL_SRCS_ENABLE_TESTED_HW})
set_property(GLOBAL PROPERTY IGDRCL_SRCS_ENABLE_TESTED_HW_LINUX ${IGDRCL_SRCS_ENABLE_TESTED_HW_LINUX})
set_property(GLOBAL PROPERTY IGDRCL_SRCS_LIB_ULT ${IGDRCL_SRCS_LIB_ULT})
set_property(GLOBAL PROPERTY IGDRCL_SRCS_LIB_ULT_ENV ${IGDRCL_SRCS_LIB_ULT_ENV})

foreach(target_name igdrcl_libult igdrcl_libult_env)
  set_target_properties(${target_name} PROPERTIES POSITION_INDEPENDENT_CODE ON)
  set_target_properties(${target_name} PROPERTIES FOLDER ${OPENCL_TEST_PROJECTS_FOLDER})
  set_property(TARGET ${target_name} APPEND_STRING PROPERTY COMPILE_FLAGS ${ASAN_FLAGS} ${TSAN_FLAGS})
  target_include_directories(${target_name} PRIVATE $<TARGET_PROPERTY:${NEO_SHARED_MOCKABLE_LIB_NAME},INTERFACE_INCLUDE_DIRECTORIES>)
  target_compile_definitions(${target_name} PRIVATE $<TARGET_PROPERTY:${NEO_SHARED_MOCKABLE_LIB_NAME},INTERFACE_COMPILE_DEFINITIONS>)
  create_project_source_tree(${target_name})
endforeach()

add_subdirectories()
