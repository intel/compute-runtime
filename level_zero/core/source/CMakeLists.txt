#
# Copyright (C) 2020-2022 Intel Corporation
#
# SPDX-License-Identifier: MIT
#

set(L0_RUNTIME_SOURCES
    ${CMAKE_CURRENT_SOURCE_DIR}/CMakeLists.txt
    ${CMAKE_CURRENT_SOURCE_DIR}/get_extension_function_lookup_map.h
    ${CMAKE_CURRENT_SOURCE_DIR}${BRANCH_DIR_SUFFIX}get_extension_function_lookup_map.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/builtin/builtin_functions_lib.h
    ${CMAKE_CURRENT_SOURCE_DIR}/builtin/builtin_functions_lib_impl.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/builtin/builtin_functions_lib_impl.h
    ${CMAKE_CURRENT_SOURCE_DIR}/cmdlist/cmdlist.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/cmdlist/cmdlist.h
    ${CMAKE_CURRENT_SOURCE_DIR}/cmdlist/cmdlist_hw.h
    ${CMAKE_CURRENT_SOURCE_DIR}/cmdlist/cmdlist_hw.inl
    ${CMAKE_CURRENT_SOURCE_DIR}/cmdlist/cmdlist_hw_base.inl
    ${CMAKE_CURRENT_SOURCE_DIR}/cmdlist/cmdlist_imp.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/cmdlist/cmdlist_imp.h
    ${CMAKE_CURRENT_SOURCE_DIR}/cmdlist/cmdlist_hw_immediate.h
    ${CMAKE_CURRENT_SOURCE_DIR}/cmdlist/cmdlist_hw_immediate.inl
    ${CMAKE_CURRENT_SOURCE_DIR}/cmdlist/cmdlist_extended${BRANCH_DIR_SUFFIX}cmdlist_extended.inl
    ${CMAKE_CURRENT_SOURCE_DIR}/cmdqueue/cmdqueue.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/cmdqueue/cmdqueue.h
    ${CMAKE_CURRENT_SOURCE_DIR}/cmdqueue/cmdqueue_hw.h
    ${CMAKE_CURRENT_SOURCE_DIR}/cmdqueue/cmdqueue_hw.inl
    ${CMAKE_CURRENT_SOURCE_DIR}/cmdqueue/cmdqueue_hw_base.inl
    ${CMAKE_CURRENT_SOURCE_DIR}/cmdqueue/cmdqueue_imp.h
    ${CMAKE_CURRENT_SOURCE_DIR}/cmdqueue/cmdqueue_extended${BRANCH_DIR_SUFFIX}cmdqueue_extended.inl
    ${CMAKE_CURRENT_SOURCE_DIR}/context/context_imp_${DRIVER_MODEL}/context_imp.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/context/context_imp.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/context/context_imp.h
    ${CMAKE_CURRENT_SOURCE_DIR}/context/context.h
    ${CMAKE_CURRENT_SOURCE_DIR}/debug_manager/debug_manager.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/device${BRANCH_DIR_SUFFIX}device_imp_helper.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/device/bcs_split.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/device/bcs_split.h
    ${CMAKE_CURRENT_SOURCE_DIR}/device/device.h
    ${CMAKE_CURRENT_SOURCE_DIR}/device/device_imp.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/device/device_imp.h
    ${CMAKE_CURRENT_SOURCE_DIR}/driver/driver_handle.h
    ${CMAKE_CURRENT_SOURCE_DIR}/driver/driver_handle_imp.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/driver/driver_handle_imp.h
    ${CMAKE_CURRENT_SOURCE_DIR}/driver/driver.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/driver/driver.h
    ${CMAKE_CURRENT_SOURCE_DIR}/driver/driver_imp.h
    ${CMAKE_CURRENT_SOURCE_DIR}/driver/host_pointer_manager.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/driver/host_pointer_manager.h
    ${CMAKE_CURRENT_SOURCE_DIR}/event/event_impl.inl
    ${CMAKE_CURRENT_SOURCE_DIR}/event/event.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/event/event.h
    ${CMAKE_CURRENT_SOURCE_DIR}/fence/fence.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/fence/fence.h
    ${CMAKE_CURRENT_SOURCE_DIR}/fabric/fabric.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/fabric/fabric.h
    ${CMAKE_CURRENT_SOURCE_DIR}/helpers/allocation_extensions.h
    ${CMAKE_CURRENT_SOURCE_DIR}/helpers/allocation_extensions.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/helpers/api_specific_config_l0.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/helpers/implicit_scaling_l0.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/helpers/l0_populate_factory.h
    ${CMAKE_CURRENT_SOURCE_DIR}/helpers/properties_parser.h
    ${CMAKE_CURRENT_SOURCE_DIR}/hw_helpers/l0_hw_helper_base.inl
    ${CMAKE_CURRENT_SOURCE_DIR}/hw_helpers/l0_hw_helper_skl_and_later.inl
    ${CMAKE_CURRENT_SOURCE_DIR}/hw_helpers/l0_hw_helper.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/hw_helpers/l0_hw_helper.h
    ${CMAKE_CURRENT_SOURCE_DIR}/kernel/kernel.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/kernel${BRANCH_DIR_SUFFIX}kernel_ext.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/kernel/kernel.h
    ${CMAKE_CURRENT_SOURCE_DIR}/kernel/kernel_hw.h
    ${CMAKE_CURRENT_SOURCE_DIR}/kernel/kernel_imp.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/kernel/kernel_imp.h
    ${CMAKE_CURRENT_SOURCE_DIR}/kernel/sampler_patch_values.h
    ${CMAKE_CURRENT_SOURCE_DIR}/image/image.h
    ${CMAKE_CURRENT_SOURCE_DIR}/image/image_format_desc_helper.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/image/image_format_desc_helper.h
    ${CMAKE_CURRENT_SOURCE_DIR}/image/image_formats.h
    ${CMAKE_CURRENT_SOURCE_DIR}/image/image_hw.h
    ${CMAKE_CURRENT_SOURCE_DIR}/image/image_hw.inl
    ${CMAKE_CURRENT_SOURCE_DIR}/image/image_imp.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/image/image_imp.h
    ${CMAKE_CURRENT_SOURCE_DIR}/memory/memory_operations_helper.h
    ${CMAKE_CURRENT_SOURCE_DIR}/memory/cpu_page_fault_memory_manager.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/memory_manager/compression_selector_l0.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/module/module.h
    ${CMAKE_CURRENT_SOURCE_DIR}/module/module_build_log.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/module/module_build_log.h
    ${CMAKE_CURRENT_SOURCE_DIR}/module/module_imp.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/module/module_imp.h
    ${CMAKE_CURRENT_SOURCE_DIR}/printf_handler/printf_handler.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/printf_handler/printf_handler.h
    ${CMAKE_CURRENT_SOURCE_DIR}/sampler/sampler.h
    ${CMAKE_CURRENT_SOURCE_DIR}/sampler/sampler_hw.h
    ${CMAKE_CURRENT_SOURCE_DIR}/sampler/sampler_hw.inl
    ${CMAKE_CURRENT_SOURCE_DIR}/sampler/sampler_imp.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/sampler/sampler_imp.h
)

if(SUPPORT_XEHP_AND_LATER)
  list(APPEND L0_RUNTIME_SOURCES
       ${CMAKE_CURRENT_SOURCE_DIR}/cmdlist/cmdlist_hw_xehp_and_later.inl
       ${CMAKE_CURRENT_SOURCE_DIR}/cmdqueue/cmdqueue_xe_hp_core_and_later.inl
  )
endif()

if(SUPPORT_GEN12LP OR SUPPORT_XE_HP_CORE OR SUPPORT_XE_HPG_CORE)
  list(APPEND L0_RUNTIME_SOURCES
       ${CMAKE_CURRENT_SOURCE_DIR}/hw_helpers/l0_hw_helper_tgllp_plus.inl
  )
endif()

if(SUPPORT_PVC_AND_LATER)
  list(APPEND L0_RUNTIME_SOURCES
       ${CMAKE_CURRENT_SOURCE_DIR}/hw_helpers/l0_hw_helper_pvc_and_later.inl
  )
endif()

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

configure_file(driver/driver_version.h.in ${CMAKE_BINARY_DIR}/driver_version_l0.h) # Put Driver version into define

# Make our source files visible to parent
set_property(GLOBAL PROPERTY L0_RUNTIME_SOURCES ${L0_RUNTIME_SOURCES})
