#
# Copyright (C) 2020-2022 Intel Corporation
#
# SPDX-License-Identifier: MIT
#

set(NEO_CORE_tests_compiler_mocks
    ${NEO_SHARED_TEST_DIRECTORY}/common/mocks/mock_cif.cpp
    ${NEO_SHARED_TEST_DIRECTORY}/common/mocks/mock_cif.h
    ${NEO_SHARED_TEST_DIRECTORY}/common/mocks/mock_compilers.cpp
    ${NEO_SHARED_TEST_DIRECTORY}/common/mocks/mock_compilers.h
)

set_property(GLOBAL PROPERTY NEO_CORE_tests_compiler_mocks ${NEO_CORE_tests_compiler_mocks})

set(NEO_CORE_tests_mocks
    ${CMAKE_CURRENT_SOURCE_DIR}/CMakeLists.txt
    ${CMAKE_CURRENT_SOURCE_DIR}/debugger_l0_create.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/mock_allocation_properties.h
    ${CMAKE_CURRENT_SOURCE_DIR}/mock_aub_center.h
    ${CMAKE_CURRENT_SOURCE_DIR}/mock_aub_csr.h
    ${CMAKE_CURRENT_SOURCE_DIR}/mock_aub_file_stream.h
    ${CMAKE_CURRENT_SOURCE_DIR}/mock_aub_manager.h
    ${CMAKE_CURRENT_SOURCE_DIR}/mock_aub_subcapture_manager.h
    ${CMAKE_CURRENT_SOURCE_DIR}/mock_bindless_heaps_helper.h
    ${CMAKE_CURRENT_SOURCE_DIR}/mock_builtins.h
    ${CMAKE_CURRENT_SOURCE_DIR}/mock_builtinslib.h
    ${CMAKE_CURRENT_SOURCE_DIR}/mock_cif.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/mock_cif.h
    ${CMAKE_CURRENT_SOURCE_DIR}/mock_command_stream_receiver.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/mock_command_stream_receiver.h
    ${CMAKE_CURRENT_SOURCE_DIR}/mock_compiler_interface_spirv.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/mock_compiler_interface_spirv.h
    ${CMAKE_CURRENT_SOURCE_DIR}/mock_compilers.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/mock_compilers.h
    ${CMAKE_CURRENT_SOURCE_DIR}/mock_cpu_page_fault_manager.h
    ${CMAKE_CURRENT_SOURCE_DIR}/mock_csr.h
    ${CMAKE_CURRENT_SOURCE_DIR}/mock_debugger.h
    ${CMAKE_CURRENT_SOURCE_DIR}/mock_deferrable_deletion.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/mock_deferrable_deletion.h
    ${CMAKE_CURRENT_SOURCE_DIR}/mock_deferred_deleter.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/mock_deferred_deleter.h
    ${CMAKE_CURRENT_SOURCE_DIR}/mock_device.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/mock_device.h
    ${CMAKE_CURRENT_SOURCE_DIR}/mock_direct_submission_hw.h
    ${CMAKE_CURRENT_SOURCE_DIR}/mock_driver_model.h
    ${CMAKE_CURRENT_SOURCE_DIR}/mock_elf.h
    ${CMAKE_CURRENT_SOURCE_DIR}/mock_execution_environment.h
    ${CMAKE_CURRENT_SOURCE_DIR}/mock_experimental_command_buffer.h
    ${CMAKE_CURRENT_SOURCE_DIR}/mock_gfx_partition.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/mock_gfx_partition.h
    ${CMAKE_CURRENT_SOURCE_DIR}/mock_gmm.h
    ${CMAKE_CURRENT_SOURCE_DIR}/mock_gmm_client_context.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/mock_gmm_client_context.h
    ${CMAKE_CURRENT_SOURCE_DIR}/mock_gmm_client_context_base.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/mock_gmm_client_context_base.h
    ${CMAKE_CURRENT_SOURCE_DIR}/mock_gmm_page_table_mngr.h
    ${CMAKE_CURRENT_SOURCE_DIR}/mock_gmm_resource_info.h
    ${CMAKE_CURRENT_SOURCE_DIR}/mock_graphics_allocation.h
    ${CMAKE_CURRENT_SOURCE_DIR}/mock_host_ptr_manager.h
    ${CMAKE_CURRENT_SOURCE_DIR}/mock_hw_helper.h
    ${CMAKE_CURRENT_SOURCE_DIR}/mock_hw_info_config.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/mock_internal_allocation_storage.h
    ${CMAKE_CURRENT_SOURCE_DIR}/mock_io_functions.h
    ${CMAKE_CURRENT_SOURCE_DIR}/mock_kernel_info.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/mock_kernel_info.h
    ${CMAKE_CURRENT_SOURCE_DIR}/mock_l0_debugger.h
    ${CMAKE_CURRENT_SOURCE_DIR}/mock_logical_state_helper.h
    ${CMAKE_CURRENT_SOURCE_DIR}/mock_memory_manager.h
    ${CMAKE_CURRENT_SOURCE_DIR}/mock_memory_operations_handler.h
    ${CMAKE_CURRENT_SOURCE_DIR}/mock_migration_sync_data.h
    ${CMAKE_CURRENT_SOURCE_DIR}/mock_modules_zebin.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/mock_modules_zebin.h
    ${CMAKE_CURRENT_SOURCE_DIR}/mock_multi_graphics_allocation.h
    ${CMAKE_CURRENT_SOURCE_DIR}/mock_os_context.h
    ${CMAKE_CURRENT_SOURCE_DIR}/mock_os_library.h
    ${CMAKE_CURRENT_SOURCE_DIR}/mock_os_library.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/mock_ostime.h
    ${CMAKE_CURRENT_SOURCE_DIR}/mock_sip.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/mock_sip.h
    ${CMAKE_CURRENT_SOURCE_DIR}/mock_source_level_debugger.h
    ${CMAKE_CURRENT_SOURCE_DIR}/mock_submissions_aggregator.h
    ${CMAKE_CURRENT_SOURCE_DIR}/mock_svm_manager.h
    ${CMAKE_CURRENT_SOURCE_DIR}/mock_tbx_csr.h
    ${CMAKE_CURRENT_SOURCE_DIR}/mock_timestamp_container.h
    ${CMAKE_CURRENT_SOURCE_DIR}/mock_timestamp_packet.h
    ${CMAKE_CURRENT_SOURCE_DIR}/ult_device_factory.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/ult_device_factory.h
)

if(WIN32)
  list(APPEND NEO_CORE_tests_mocks
       ${CMAKE_CURRENT_SOURCE_DIR}/mock_ostime_win.h
       ${CMAKE_CURRENT_SOURCE_DIR}/mock_wddm.h
       ${CMAKE_CURRENT_SOURCE_DIR}/mock_wddm_interface20.h
       ${CMAKE_CURRENT_SOURCE_DIR}/mock_wddm_interface23.h
       ${CMAKE_CURRENT_SOURCE_DIR}/mock_wddm_residency_allocations_container.h
       ${CMAKE_CURRENT_SOURCE_DIR}/mock_wddm_residency_logger.h
       ${CMAKE_CURRENT_SOURCE_DIR}/wddm_mock_helpers.h
       ${CMAKE_CURRENT_SOURCE_DIR}/windows/mock_gdi_interface.cpp
       ${CMAKE_CURRENT_SOURCE_DIR}/windows/mock_gdi_interface.h
       ${CMAKE_CURRENT_SOURCE_DIR}/windows/mock_wddm_eudebug.h
  )
else()
  list(APPEND NEO_CORE_tests_mocks
       ${CMAKE_CURRENT_SOURCE_DIR}/linux/mock_dlopen.cpp
       ${CMAKE_CURRENT_SOURCE_DIR}/linux/mock_drm_allocation.h
       ${CMAKE_CURRENT_SOURCE_DIR}/linux/mock_drm_command_stream_receiver.h
       ${CMAKE_CURRENT_SOURCE_DIR}/linux/mock_drm_memory_manager.h
       ${CMAKE_CURRENT_SOURCE_DIR}/linux/mock_drm_wrappers.cpp
       ${CMAKE_CURRENT_SOURCE_DIR}/linux/mock_drm_wrappers.h
       ${CMAKE_CURRENT_SOURCE_DIR}/linux/mock_os_time_linux.h
  )
endif()

if(TESTS_XEHP_AND_LATER)
  list(APPEND NEO_CORE_tests_mocks
       ${CMAKE_CURRENT_SOURCE_DIR}/mock_scratch_space_controller_xehp_and_later.h
  )
endif()

add_library(neo_shared_mocks OBJECT EXCLUDE_FROM_ALL ${NEO_CORE_tests_mocks})

set_target_properties(neo_shared_mocks PROPERTIES POSITION_INDEPENDENT_CODE ON)
set_target_properties(neo_shared_mocks PROPERTIES FOLDER ${SHARED_TEST_PROJECTS_FOLDER})
set_property(TARGET neo_shared_mocks APPEND_STRING PROPERTY COMPILE_FLAGS ${ASAN_FLAGS} ${TSAN_FLAGS})
target_include_directories(neo_shared_mocks PRIVATE
                           $<TARGET_PROPERTY:${NEO_SHARED_MOCKABLE_LIB_NAME},INTERFACE_INCLUDE_DIRECTORIES>
                           $<TARGET_PROPERTY:gmock-gtest,INTERFACE_INCLUDE_DIRECTORIES>
                           ${SOURCE_LEVEL_DEBUGGER_HEADERS_DIR}
)
target_compile_definitions(neo_shared_mocks PRIVATE $<TARGET_PROPERTY:${NEO_SHARED_MOCKABLE_LIB_NAME},INTERFACE_COMPILE_DEFINITIONS>)
create_project_source_tree(neo_shared_mocks)