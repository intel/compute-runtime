#
# Copyright (C) 2021-2025 Intel Corporation
#
# SPDX-License-Identifier: MIT
#

macro(macro_for_each_platform)
  string(TOLOWER ${PLATFORM_IT} PLATFORM_IT_LOWER)

  foreach(BRANCH_DIR ${BRANCH_DIR_LIST})
    foreach(BRANCH ${BRANCH_DIR_LIST})
      set(SRC_FILE ${NEO_SHARED_DIRECTORY}${BRANCH_DIR}${CORE_TYPE_LOWER}${BRANCH}enable_product_helper_${PLATFORM_IT_LOWER}.cpp)
      if(EXISTS ${SRC_FILE})
        list(APPEND neo_libult_common_SRCS_ENABLE_TESTED_HW ${SRC_FILE})
      endif()
      set(SRC_FILE ${NEO_SHARED_DIRECTORY}${BRANCH_DIR}${CORE_TYPE_LOWER}${BRANCH}enable_compiler_product_helper_${PLATFORM_IT_LOWER}.cpp)
      if(EXISTS ${SRC_FILE})
        list(APPEND neo_libult_common_SRCS_ENABLE_TESTED_HW ${SRC_FILE})
      endif()
      set(SRC_FILE ${NEO_SHARED_DIRECTORY}/os_interface/linux/local${BRANCH_DIR_SUFFIX}${PLATFORM_IT_LOWER}/enable_ioctl_helper_${PLATFORM_IT_LOWER}.cpp)
      if(EXISTS ${SRC_FILE})
        list(APPEND neo_libult_common_SRCS_ENABLE_TESTED_HW_LINUX ${SRC_FILE})
      endif()
      set(SRC_FILE ${NEO_SHARED_DIRECTORY}/ail${BRANCH_DIR}${CORE_TYPE_LOWER}${BRANCH}${PLATFORM_IT_LOWER}/ail_configuration_${PLATFORM_IT_LOWER}.cpp)
      if(EXISTS ${SRC_FILE})
        list(APPEND neo_libult_common_SRCS_ENABLE_TESTED_HW ${SRC_FILE})
      endif()
      set(SRC_FILE ${NEO_SHARED_TEST_DIRECTORY}/common${BRANCH_DIR}${CORE_TYPE_LOWER}${BRANCH}${PLATFORM_IT_LOWER}/mock_product_helper_${PLATFORM_IT_LOWER}.cpp)
      if(EXISTS ${SRC_FILE})
        list(APPEND neo_libult_common_SRCS_ENABLE_TESTED_HW ${SRC_FILE})
      endif()
    endforeach()
  endforeach()
endmacro()

macro(macro_for_each_core_type)
  foreach(BRANCH_DIR ${BRANCH_DIR_LIST})
    foreach(BRANCH ${BRANCH_DIR_LIST})
      set(SRC_FILE ${NEO_SHARED_TEST_DIRECTORY}/common/${BRANCH}${CORE_TYPE_LOWER}/cmd_parse_${CORE_TYPE_LOWER}.cpp)
      if(EXISTS ${SRC_FILE})
        list(APPEND neo_libult_common_SRCS_ENABLE_TESTED_HW ${SRC_FILE})
      endif()
      set(SRC_FILE ${NEO_SHARED_TEST_DIRECTORY}/common/libult/${BRANCH}${CORE_TYPE_LOWER}.cpp)
      if(EXISTS ${SRC_FILE})
        list(APPEND neo_libult_common_SRCS_ENABLE_TESTED_HW ${SRC_FILE})
      endif()
      set(SRC_FILE ${NEO_SHARED_TEST_DIRECTORY}/common/${BRANCH}${CORE_TYPE_LOWER}/unit_test_helper_${CORE_TYPE_LOWER}.cpp)
      if(EXISTS ${SRC_FILE})
        list(APPEND neo_libult_common_SRCS_ENABLE_TESTED_HW ${SRC_FILE})
        target_include_directories(neo_libult_common PRIVATE ${NEO_SHARED_TEST_DIRECTORY}/common/${BRANCH}${CORE_TYPE_LOWER}/definitions${BRANCH_DIR_SUFFIX})
      endif()
      foreach(SRC_IT "enable_family_full_core_" "implicit_scaling_")
        set(SRC_FILE ${NEO_SHARED_DIRECTORY}${BRANCH}${CORE_TYPE_LOWER}${BRANCH_DIR}${SRC_IT}${CORE_TYPE_LOWER}.cpp)
        if(EXISTS ${SRC_FILE})
          list(APPEND neo_libult_common_SRCS_ENABLE_TESTED_HW ${SRC_FILE})
        endif()
        set(SRC_FILE ${NEO_SHARED_DIRECTORY}${BRANCH_DIR}${CORE_TYPE_LOWER}/${SRC_IT}${CORE_TYPE_LOWER}.cpp)
        if(EXISTS ${SRC_FILE})
          list(APPEND neo_libult_common_SRCS_ENABLE_TESTED_HW ${SRC_FILE})
        endif()
      endforeach()
    endforeach()
  endforeach()

  apply_macro_for_each_platform("TESTED")
endmacro()

apply_macro_for_each_core_type("TESTED")

append_sources_from_properties(neo_libult_common_SRCS_ENABLE_TESTED_HW NEO_SRCS_ENABLE_CORE)

set(neo_libult_common_SRCS_LIB_ULT
    ${NEO_SHARED_DIRECTORY}/gmm_helper/resource_info_impl.cpp
    ${NEO_SHARED_TEST_DIRECTORY}/common/base_ult_config_listener.cpp
    ${NEO_SHARED_TEST_DIRECTORY}/common/base_ult_config_listener.h
    ${NEO_SHARED_TEST_DIRECTORY}/common/fixtures/aub_command_stream_receiver_fixture.h
    ${NEO_SHARED_TEST_DIRECTORY}/common/fixtures/command_stream_receiver_fixture.cpp
    ${NEO_SHARED_TEST_DIRECTORY}/common/fixtures/command_stream_receiver_fixture.h
    ${NEO_SHARED_TEST_DIRECTORY}/common/fixtures/command_stream_receiver_fixture.inl
    ${NEO_SHARED_TEST_DIRECTORY}/common/fixtures/device_fixture.cpp
    ${NEO_SHARED_TEST_DIRECTORY}/common/fixtures/device_fixture.h
    ${NEO_SHARED_TEST_DIRECTORY}/common/fixtures/gmm_callbacks_fixture.h
    ${NEO_SHARED_TEST_DIRECTORY}/common/fixtures/memory_allocator_fixture.h
    ${NEO_SHARED_TEST_DIRECTORY}/common/fixtures/memory_allocator_multi_device_fixture.h
    ${NEO_SHARED_TEST_DIRECTORY}/common/fixtures/memory_management_fixture.cpp
    ${NEO_SHARED_TEST_DIRECTORY}/common/fixtures/memory_management_fixture.h
    ${NEO_SHARED_TEST_DIRECTORY}/common/fixtures/memory_manager_fixture.cpp
    ${NEO_SHARED_TEST_DIRECTORY}/common/fixtures/memory_manager_fixture.h
    ${NEO_SHARED_TEST_DIRECTORY}/common/fixtures/mock_execution_environment_gmm_fixture.cpp
    ${NEO_SHARED_TEST_DIRECTORY}/common/fixtures/mock_execution_environment_gmm_fixture.h
    ${NEO_SHARED_TEST_DIRECTORY}/common/fixtures/tbx_command_stream_fixture.cpp
    ${NEO_SHARED_TEST_DIRECTORY}/common/fixtures/tbx_command_stream_fixture.h
    ${NEO_SHARED_TEST_DIRECTORY}/common/helpers/debug_helpers.cpp
    ${NEO_SHARED_TEST_DIRECTORY}/common/helpers/execution_environment_helper.cpp
    ${NEO_SHARED_TEST_DIRECTORY}/common/helpers/execution_environment_helper.h
    ${NEO_SHARED_TEST_DIRECTORY}/common/helpers/kernel_binary_helper.cpp
    ${NEO_SHARED_TEST_DIRECTORY}/common/helpers/kernel_binary_helper.h
    ${NEO_SHARED_TEST_DIRECTORY}/common/helpers/kernel_binary_helper_hash_value.cpp
    ${NEO_SHARED_TEST_DIRECTORY}/common/helpers/kernel_filename_helper.h
    ${NEO_SHARED_TEST_DIRECTORY}/common/helpers/memory_leak_listener.cpp
    ${NEO_SHARED_TEST_DIRECTORY}/common/helpers/memory_leak_listener.h
    ${NEO_SHARED_TEST_DIRECTORY}/common/helpers/memory_management.cpp
    ${NEO_SHARED_TEST_DIRECTORY}/common/helpers/memory_management.h
    ${NEO_SHARED_TEST_DIRECTORY}/common/helpers/mock_product_helper_hw.cpp
    ${NEO_SHARED_TEST_DIRECTORY}/common/helpers/mock_product_helper_hw.h
    ${NEO_SHARED_TEST_DIRECTORY}/common/helpers/mock_product_helper_hw.inl
    ${NEO_SHARED_TEST_DIRECTORY}/common/helpers/sip_init.cpp
    ${NEO_SHARED_TEST_DIRECTORY}/common/helpers/test_files.cpp
    ${NEO_SHARED_TEST_DIRECTORY}/common/helpers/test_files.h
    ${NEO_SHARED_TEST_DIRECTORY}/common/helpers/unit_test_helper.h
    ${NEO_SHARED_TEST_DIRECTORY}/common/helpers/unit_test_helper.inl
    ${NEO_SHARED_TEST_DIRECTORY}/common/helpers${BRANCH_DIR_SUFFIX}unit_test_setter.cpp
    ${NEO_SHARED_TEST_DIRECTORY}/common/helpers/file_io.cpp
    ${NEO_SHARED_TEST_DIRECTORY}/common/libult/abort.cpp
    ${NEO_SHARED_TEST_DIRECTORY}/common/libult/create_tbx_sockets.cpp
    ${NEO_SHARED_TEST_DIRECTORY}/common/libult/debug_settings_reader_creator.cpp
    ${NEO_SHARED_TEST_DIRECTORY}/common/libult/global_environment.cpp
    ${NEO_SHARED_TEST_DIRECTORY}/common/libult/global_environment.h
    ${NEO_SHARED_TEST_DIRECTORY}/common/libult/mock_io_functions.cpp
    ${NEO_SHARED_TEST_DIRECTORY}/common/libult/options_unit_tests.cpp
    ${NEO_SHARED_TEST_DIRECTORY}/common/libult/signal_utils.h
    ${NEO_SHARED_TEST_DIRECTORY}/common/libult/sleep.cpp
    ${NEO_SHARED_TEST_DIRECTORY}/common/libult/ult_aub_command_stream_receiver.h
    ${NEO_SHARED_TEST_DIRECTORY}/common/libult/ult_command_stream_receiver.h
    ${NEO_SHARED_TEST_DIRECTORY}/common/mocks/mock_gmm_page_table_mngr.cpp
    ${NEO_SHARED_TEST_DIRECTORY}/common/mocks/mock_memory_manager.cpp
    ${NEO_SHARED_TEST_DIRECTORY}/common/test_macros/test_excludes.cpp
    ${NEO_SHARED_TEST_DIRECTORY}/common/test_macros/test_excludes.h
    ${NEO_SHARED_TEST_DIRECTORY}/common/test_macros/test_checks_shared.cpp
    ${NEO_SHARED_TEST_DIRECTORY}/common/test_macros/test_checks_shared.h
    ${NEO_SHARED_TEST_DIRECTORY}/common/utilities/cpuintrinsics.cpp
)

if(TESTS_XEHP_AND_LATER)
  list(APPEND neo_libult_common_SRCS_LIB_ULT
       ${NEO_SHARED_TEST_DIRECTORY}/common/helpers/unit_test_helper_xehp_and_later.inl
  )
endif()

if(TESTS_XE_HPG_CORE OR TESTS_XE_HPC_CORE)
  list(APPEND neo_libult_common_SRCS_LIB_ULT
       ${NEO_SHARED_TEST_DIRECTORY}/common/helpers/unit_test_helper_xe_hpg_and_xe_hpc.inl
  )
endif()

if(TESTS_PVC_AND_LATER)
  list(APPEND neo_libult_common_SRCS_LIB_ULT
       ${NEO_SHARED_TEST_DIRECTORY}/common/helpers/unit_test_helper_xe_hpc_and_later.inl
  )
endif()

if(TESTS_XE2_AND_LATER)
  list(APPEND neo_libult_common_SRCS_LIB_ULT
       ${NEO_SHARED_TEST_DIRECTORY}/common/helpers/unit_test_helper_xe2_and_later.inl
  )
endif()

append_sources_from_properties(neo_libult_common_SRCS_LIB_ULT NEO_CORE_ENABLE_RELEASE_HELPER)

set_property(GLOBAL PROPERTY neo_libult_common_SRCS_LIB_ULT ${neo_libult_common_SRCS_LIB_ULT})

set_property(GLOBAL PROPERTY neo_libult_common_SRCS_ENABLE_TESTED_HW ${neo_libult_common_SRCS_ENABLE_TESTED_HW})

set_property(GLOBAL PROPERTY neo_libult_common_SRCS_ENABLE_TESTED_HW_LINUX ${neo_libult_common_SRCS_ENABLE_TESTED_HW_LINUX})

target_sources(neo_libult_common PRIVATE
               ${neo_libult_common_SRCS_LIB_ULT}
               ${neo_libult_common_SRCS_ENABLE_TESTED_HW}
)

set(neo_libult_common_SRCS_LIB_ULT_WIN
    ${NEO_SHARED_TEST_DIRECTORY}/common/fixtures/windows/memory_allocator_multi_device_fixture_windows.cpp
    ${NEO_SHARED_TEST_DIRECTORY}/common/os_interface/windows/ult_dxgi_factory.cpp
    ${NEO_SHARED_TEST_DIRECTORY}/common/os_interface/windows/ult_dxgi_factory.h
    ${NEO_SHARED_TEST_DIRECTORY}/common/os_interface/windows/ult_dxcore_factory.cpp
    ${NEO_SHARED_TEST_DIRECTORY}/common/os_interface/windows/ult_dxcore_factory.h
    ${NEO_SHARED_TEST_DIRECTORY}/common/os_interface/windows/signal_utils.cpp
)
set_property(GLOBAL PROPERTY neo_libult_common_SRCS_LIB_ULT_WIN ${neo_libult_common_SRCS_LIB_ULT_WIN})

set(neo_libult_common_SRCS_LIB_ULT_LINUX
    ${NEO_SHARED_TEST_DIRECTORY}/common/fixtures/linux/memory_allocator_multi_device_fixture_linux.cpp
    ${NEO_SHARED_TEST_DIRECTORY}/common/os_interface/linux/device_command_stream_fixture.cpp
    ${NEO_SHARED_TEST_DIRECTORY}/common/os_interface/linux/device_command_stream_fixture.h
    ${NEO_SHARED_TEST_DIRECTORY}/common/os_interface/linux/drm_buffer_object_fixture.h
    ${NEO_SHARED_TEST_DIRECTORY}/common/os_interface/linux/signal_utils.cpp
)
set_property(GLOBAL PROPERTY neo_libult_common_SRCS_LIB_ULT_LINUX ${neo_libult_common_SRCS_LIB_ULT_LINUX})

if(WIN32)
  target_sources(neo_libult_common PRIVATE ${neo_libult_common_SRCS_LIB_ULT_WIN})
elseif(UNIX)
  target_sources(neo_libult_common PRIVATE ${neo_libult_common_SRCS_LIB_ULT_LINUX})
  target_sources(neo_libult_common PRIVATE ${neo_libult_common_SRCS_ENABLE_TESTED_HW_LINUX})
endif()

set(neo_libult_SRCS_LINUX
    ${NEO_SHARED_TEST_DIRECTORY}/common/libult/create_directory.cpp
    ${NEO_SHARED_TEST_DIRECTORY}/common/libult/os_interface.cpp
    ${NEO_SHARED_TEST_DIRECTORY}/common/mocks/linux/mock_drm_memory_manager.cpp
    ${NEO_SHARED_TEST_DIRECTORY}/common/mocks/linux/mock_drm_memory_manager.h
    ${NEO_SHARED_TEST_DIRECTORY}/common/mocks/device_ult.cpp
    ${NEO_SHARED_TEST_DIRECTORY}/common/mocks/mock_direct_submission_controller_enabled.cpp
    ${NEO_SHARED_TEST_DIRECTORY}/common/mocks/mock_unified_memory_reuse_cleaner_enabled.cpp
    ${NEO_SHARED_TEST_DIRECTORY}/common/os_interface/linux/create_drm_memory_manager.cpp
    ${NEO_SHARED_TEST_DIRECTORY}/common/os_interface/linux/drm_memory_manager_fixture.cpp
    ${NEO_SHARED_TEST_DIRECTORY}/common/os_interface/linux/drm_neo_create.cpp
    ${NEO_SHARED_TEST_DIRECTORY}/common/os_interface/linux${BRANCH_DIR_SUFFIX}/options.cpp
    ${NEO_SHARED_TEST_DIRECTORY}/common/os_interface/linux/options.inl
    ${NEO_SHARED_TEST_DIRECTORY}/common/os_interface/setup_external_dependencies_${DRIVER_MODEL}.cpp
    ${NEO_SHARED_TEST_DIRECTORY}/common/os_interface/linux/sys_calls_linux_ult.cpp
    ${NEO_SHARED_TEST_DIRECTORY}/common/mocks/mock_get_staging_buffer_size.cpp
    ${NEO_SHARED_TEST_DIRECTORY}/common/mocks/mock_usm_pool_size.cpp
)

set_property(GLOBAL APPEND PROPERTY neo_libult_SRCS_LINUX ${neo_libult_SRCS_LINUX})
set(neo_libult_SRCS_WINDOWS
    ${NEO_SHARED_TEST_DIRECTORY}/common/libult/os_interface.cpp
    ${NEO_SHARED_TEST_DIRECTORY}/common/mocks/device_ult.cpp
    ${NEO_SHARED_TEST_DIRECTORY}/common/mocks/mock_direct_submission_controller_enabled.cpp
    ${NEO_SHARED_TEST_DIRECTORY}/common/mocks/mock_unified_memory_reuse_cleaner_enabled.cpp
    ${NEO_SHARED_TEST_DIRECTORY}/common/os_interface/windows/create_wddm_memory_manager.cpp
    ${NEO_SHARED_TEST_DIRECTORY}/common/os_interface/windows${BRANCH_DIR_SUFFIX}/options.cpp
    ${NEO_SHARED_TEST_DIRECTORY}/common/os_interface/windows/options.inl
    ${NEO_SHARED_TEST_DIRECTORY}/common/os_interface/setup_external_dependencies_drm_or_wddm.cpp
    ${NEO_SHARED_TEST_DIRECTORY}/common/os_interface/windows/sys_calls.cpp
    ${NEO_SHARED_TEST_DIRECTORY}/common/os_interface/windows/sys_calls_winmm.cpp
    ${NEO_SHARED_TEST_DIRECTORY}/common/os_interface/windows/wddm_calls.cpp
    ${NEO_SHARED_TEST_DIRECTORY}/common/mocks/mock_get_staging_buffer_size.cpp
    ${NEO_SHARED_TEST_DIRECTORY}/common/mocks/mock_usm_pool_size.cpp
)

set_property(GLOBAL APPEND PROPERTY neo_libult_SRCS_WINDOWS ${neo_libult_SRCS_WINDOWS})
if(UNIX)
  set(neo_libult_SRCS ${neo_libult_SRCS_LINUX})
else()
  set(neo_libult_SRCS ${neo_libult_SRCS_WINDOWS})
endif()

if(WIN32 OR NOT DISABLE_WDDM_LINUX)
  list(APPEND neo_libult_SRCS
       ${NEO_SHARED_TEST_DIRECTORY}/common/mocks/mock_wddm.cpp
       ${NEO_SHARED_TEST_DIRECTORY}/common/os_interface/windows/wddm_create.cpp
  )
endif()

if(UNIX AND NOT DISABLE_WDDM_LINUX)
  list(APPEND neo_libult_SRCS
       ${NEO_SHARED_TEST_DIRECTORY}/common/mocks/linux/mock_dxcore.h
       ${NEO_SHARED_TEST_DIRECTORY}/common/os_interface/linux/sys_calls_win.cpp
       ${NEO_SHARED_TEST_DIRECTORY}/common/os_interface/windows/ult_dxcore_factory.cpp
       ${NEO_SHARED_TEST_DIRECTORY}/common/os_interface/windows/ult_dxcore_factory.h
  )
endif()

add_library(neo_libult OBJECT EXCLUDE_FROM_ALL ${neo_libult_SRCS})

add_subdirectories()

foreach(target_name "neo_libult_common" "neo_libult")
  set_target_properties(${target_name} PROPERTIES POSITION_INDEPENDENT_CODE ON)
  set_target_properties(${target_name} PROPERTIES FOLDER ${SHARED_TEST_PROJECTS_FOLDER})
  set_property(TARGET ${target_name} APPEND_STRING PROPERTY COMPILE_FLAGS ${ASAN_FLAGS} ${TSAN_FLAGS})
  target_include_directories(${target_name} PRIVATE
                             ${NEO_SHARED_TEST_DIRECTORY}/common/test_configuration/unit_tests
                             $<TARGET_PROPERTY:${NEO_SHARED_MOCKABLE_LIB_NAME},INTERFACE_INCLUDE_DIRECTORIES>
                             $<TARGET_PROPERTY:gmock-gtest,INTERFACE_INCLUDE_DIRECTORIES>
  )
  target_compile_definitions(${target_name} PRIVATE $<TARGET_PROPERTY:${NEO_SHARED_MOCKABLE_LIB_NAME},INTERFACE_COMPILE_DEFINITIONS>)
  # please do not add new sources below this function
  create_project_source_tree(${target_name})
endforeach()
