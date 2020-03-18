#
# Copyright (C) 2019-2020 Intel Corporation
#
# SPDX-License-Identifier: MIT
#

set(L0_RUNTIME_SOURCES
    ${CMAKE_CURRENT_SOURCE_DIR}/CMakeLists.txt
    ${CMAKE_CURRENT_SOURCE_DIR}${BRANCH_DIR_SUFFIX}/additional_kernel_properties.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/builtin_functions_lib.h
    ${CMAKE_CURRENT_SOURCE_DIR}/builtin_functions_lib_impl.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/builtin_functions_lib_impl.h
    ${CMAKE_CURRENT_SOURCE_DIR}/cmdlist.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/cmdlist.h
    ${CMAKE_CURRENT_SOURCE_DIR}/cmdlist_hw.h
    ${CMAKE_CURRENT_SOURCE_DIR}/cmdlist_hw.inl
    ${CMAKE_CURRENT_SOURCE_DIR}/cmdlist_hw_base.inl
    ${CMAKE_CURRENT_SOURCE_DIR}/cmdlist_imp.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/cmdlist_imp.h
    ${CMAKE_CURRENT_SOURCE_DIR}/cmdqueue.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/cmdqueue.h
    ${CMAKE_CURRENT_SOURCE_DIR}/cmdqueue_hw.h
    ${CMAKE_CURRENT_SOURCE_DIR}/cmdlist_hw_immediate.h
    ${CMAKE_CURRENT_SOURCE_DIR}/cmdlist_hw_immediate.inl
    ${CMAKE_CURRENT_SOURCE_DIR}/cmdqueue_hw.inl
    ${CMAKE_CURRENT_SOURCE_DIR}/cmdqueue_hw_base.inl
    ${CMAKE_CURRENT_SOURCE_DIR}/cmdqueue_imp.h
    ${CMAKE_CURRENT_SOURCE_DIR}/cpu_page_fault_memory_manager.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/debug_manager.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/debugger.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/device.h
    ${CMAKE_CURRENT_SOURCE_DIR}/driver_handle.h
    ${CMAKE_CURRENT_SOURCE_DIR}/device_imp.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/driver_handle_imp.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/device_imp.h
    ${CMAKE_CURRENT_SOURCE_DIR}/driver_handle_imp.h
    ${CMAKE_CURRENT_SOURCE_DIR}/driver.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/driver.h
    ${CMAKE_CURRENT_SOURCE_DIR}/driver_imp.h
    ${CMAKE_CURRENT_SOURCE_DIR}/event.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/event.h
    ${CMAKE_CURRENT_SOURCE_DIR}/fence.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/fence.h
    ${CMAKE_CURRENT_SOURCE_DIR}/kernel.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/kernel.h
    ${CMAKE_CURRENT_SOURCE_DIR}/kernel_hw.h
    ${CMAKE_CURRENT_SOURCE_DIR}/kernel_imp.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/kernel_imp.h
    ${CMAKE_CURRENT_SOURCE_DIR}/image.h
    ${CMAKE_CURRENT_SOURCE_DIR}/image_hw.h
    ${CMAKE_CURRENT_SOURCE_DIR}/image_hw.inl
    ${CMAKE_CURRENT_SOURCE_DIR}/image_imp.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/image_imp.h
    ${CMAKE_CURRENT_SOURCE_DIR}/memory.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/memory_operations_helper.h
    ${CMAKE_CURRENT_SOURCE_DIR}/module.h
    ${CMAKE_CURRENT_SOURCE_DIR}/module_build_log.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/module_build_log.h
    ${CMAKE_CURRENT_SOURCE_DIR}${BRANCH_DIR_SUFFIX}/module_extra_options.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/module_imp.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/module_imp.h
    ${CMAKE_CURRENT_SOURCE_DIR}/printf_handler.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/printf_handler.h
    ${CMAKE_CURRENT_SOURCE_DIR}/sampler.h
    ${CMAKE_CURRENT_SOURCE_DIR}/sampler_hw.h
    ${CMAKE_CURRENT_SOURCE_DIR}/sampler_hw.inl
    ${CMAKE_CURRENT_SOURCE_DIR}/sampler_imp.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/sampler_imp.h
    ${CMAKE_CURRENT_SOURCE_DIR}/hw_helpers${BRANCH_DIR_SUFFIX}/hw_helpers.h
    ${CMAKE_CURRENT_SOURCE_DIR}/cmdlist_extended${BRANCH_DIR_SUFFIX}/cmdlist_extended.inl
    ${CMAKE_CURRENT_SOURCE_DIR}/cmdqueue_extended${BRANCH_DIR_SUFFIX}/cmdqueue_extended.inl
)

target_include_directories(${L0_STATIC_LIB_NAME}
    PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/
)

target_include_directories(${TARGET_NAME_L0}
    PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/
)

add_subdirectories()
include_directories(${CMAKE_CURRENT_SOURCE_DIR})

target_sources(${L0_STATIC_LIB_NAME}
    PRIVATE
        ${L0_RUNTIME_SOURCES}
        ${CMAKE_CURRENT_SOURCE_DIR}/CMakeLists.txt
)

target_sources(${TARGET_NAME_L0}
    PRIVATE
        ${L0_SRCS_DLL}
        ${CMAKE_CURRENT_SOURCE_DIR}/CMakeLists.txt
)

if(UNIX)
    set_source_files_properties(${L0_RUNTIME_SOURCES} PROPERTIES COMPILE_FLAGS -Wall)
endif()

if(NOT DEFINED L0_DRIVER_VERSION)
  set(L0_DRIVER_VERSION 1)
endif()

configure_file(driver_version.h.in ${CMAKE_BINARY_DIR}/driver_version_l0.h) # Put Driver version into define

# Make our source files visible to parent
set_property(GLOBAL PROPERTY L0_RUNTIME_SOURCES ${L0_RUNTIME_SOURCES})
