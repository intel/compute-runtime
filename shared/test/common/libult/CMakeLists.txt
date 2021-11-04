#
# Copyright (C) 2021 Intel Corporation
#
# SPDX-License-Identifier: MIT
#

macro(macro_for_each_platform)
  string(TOLOWER ${PLATFORM_IT} PLATFORM_IT_LOWER)

  foreach(BRANCH_DIR ${BRANCH_DIR_LIST})
    foreach(BRANCH ${BRANCH_DIR_LIST})
      set(SRC_FILE ${NEO_SHARED_DIRECTORY}${BRANCH_DIR}${GEN_TYPE_LOWER}${BRANCH}enable_hw_info_config_${PLATFORM_IT_LOWER}.cpp)
      if(EXISTS ${SRC_FILE})
        list(APPEND neo_libult_common_SRCS_ENABLE_TESTED_HW ${SRC_FILE})
      endif()
      set(SRC_FILE ${NEO_SHARED_DIRECTORY}/os_interface/linux/local${BRANCH_DIR_SUFFIX}${PLATFORM_IT_LOWER}/enable_local_memory_helper_${PLATFORM_IT_LOWER}.cpp)
      if(EXISTS ${SRC_FILE})
        list(APPEND neo_libult_common_SRCS_ENABLE_TESTED_HW_LINUX ${SRC_FILE})
      endif()
      set(SRC_FILE ${NEO_SHARED_DIRECTORY}/ail${BRANCH_DIR}${GEN_TYPE_LOWER}${BRANCH}${PLATFORM_IT_LOWER}/ail_configuration_${PLATFORM_IT_LOWER}.cpp)
      if(EXISTS ${SRC_FILE})
        list(APPEND neo_libult_common_SRCS_ENABLE_TESTED_HW ${SRC_FILE})
      endif()
    endforeach()
  endforeach()
endmacro()

macro(macro_for_each_gen)
  foreach(BRANCH_DIR ${BRANCH_DIR_LIST})
    foreach(BRANCH ${BRANCH_DIR_LIST})
      set(SRC_FILE ${NEO_SHARED_TEST_DIRECTORY}/common/${BRANCH}${GEN_TYPE_LOWER}/cmd_parse_${GEN_TYPE_LOWER}.cpp)
      if(EXISTS ${SRC_FILE})
        list(APPEND neo_libult_common_SRCS_ENABLE_TESTED_HW ${SRC_FILE})
      endif()
      set(SRC_FILE ${NEO_SHARED_TEST_DIRECTORY}/common/libult/${BRANCH}${GEN_TYPE_LOWER}.cpp)
      if(EXISTS ${SRC_FILE})
        list(APPEND neo_libult_common_SRCS_ENABLE_TESTED_HW ${SRC_FILE})
      endif()
      foreach(SRC_IT "enable_" "enable_family_full_core_" "implicit_scaling_")
        set(SRC_FILE ${NEO_SHARED_DIRECTORY}${BRANCH}${GEN_TYPE_LOWER}${BRANCH_DIR}${SRC_IT}${GEN_TYPE_LOWER}.cpp)
        if(EXISTS ${SRC_FILE})
          list(APPEND neo_libult_common_SRCS_ENABLE_TESTED_HW ${SRC_FILE})
        endif()
        set(SRC_FILE ${NEO_SHARED_DIRECTORY}${BRANCH_DIR}${GEN_TYPE_LOWER}/${SRC_IT}${GEN_TYPE_LOWER}.cpp)
        if(EXISTS ${SRC_FILE})
          list(APPEND neo_libult_common_SRCS_ENABLE_TESTED_HW ${SRC_FILE})
        endif()
      endforeach()
    endforeach()
  endforeach()

  apply_macro_for_each_platform("TESTED")
endmacro()

apply_macro_for_each_gen("TESTED")

set(neo_libult_common_SRCS_LIB_ULT
    ${NEO_SHARED_TEST_DIRECTORY}/common/fixtures/device_fixture.cpp
    ${NEO_SHARED_TEST_DIRECTORY}/common/fixtures/device_fixture.h
    ${NEO_SHARED_TEST_DIRECTORY}/common/fixtures/mock_execution_environment_gmm_fixture.cpp
    ${NEO_SHARED_TEST_DIRECTORY}/common/fixtures/mock_execution_environment_gmm_fixture.h
    ${NEO_SHARED_TEST_DIRECTORY}/common/helpers${BRANCH_DIR_SUFFIX}kernel_binary_helper_hash_value.cpp
    ${NEO_SHARED_TEST_DIRECTORY}/common/helpers/kernel_binary_helper.cpp
    ${NEO_SHARED_TEST_DIRECTORY}/common/helpers/kernel_binary_helper.h
    ${NEO_SHARED_TEST_DIRECTORY}/common/helpers/kernel_filename_helper.h
    ${NEO_SHARED_TEST_DIRECTORY}/common/helpers/memory_leak_listener.cpp
    ${NEO_SHARED_TEST_DIRECTORY}/common/helpers/memory_leak_listener.h
    ${NEO_SHARED_TEST_DIRECTORY}/common/helpers/memory_management.cpp
    ${NEO_SHARED_TEST_DIRECTORY}/common/helpers/memory_management.h
    ${NEO_SHARED_TEST_DIRECTORY}/common/fixtures/memory_management_fixture.cpp
    ${NEO_SHARED_TEST_DIRECTORY}/common/fixtures/memory_management_fixture.h
    ${NEO_SHARED_TEST_DIRECTORY}/common/helpers/sip_init.cpp
    ${NEO_SHARED_TEST_DIRECTORY}/common/helpers/test_files.cpp
    ${NEO_SHARED_TEST_DIRECTORY}/common/helpers/test_files.h
    ${NEO_SHARED_TEST_DIRECTORY}/common/libult/abort.cpp
    ${NEO_SHARED_TEST_DIRECTORY}/common/libult/create_tbx_sockets.cpp
    ${NEO_SHARED_TEST_DIRECTORY}/common/libult/debug_settings_reader_creator.cpp
    ${NEO_SHARED_TEST_DIRECTORY}/common/libult/global_environment.cpp
    ${NEO_SHARED_TEST_DIRECTORY}/common/libult/global_environment.h
    ${NEO_SHARED_TEST_DIRECTORY}/common/libult/io_functions.cpp
    ${NEO_SHARED_TEST_DIRECTORY}/common/libult/options_unit_tests.cpp
    ${NEO_SHARED_TEST_DIRECTORY}/common/libult/source_level_debugger_library.cpp
    ${NEO_SHARED_TEST_DIRECTORY}/common/libult/source_level_debugger_library.h
    ${NEO_SHARED_TEST_DIRECTORY}/common/libult/source_level_debugger_ult.cpp
    ${NEO_SHARED_TEST_DIRECTORY}/common/libult/ult_aub_command_stream_receiver.h
    ${NEO_SHARED_TEST_DIRECTORY}/common/libult/ult_command_stream_receiver.h
    ${NEO_SHARED_TEST_DIRECTORY}/common/mocks/mock_memory_manager.cpp
    ${NEO_SHARED_TEST_DIRECTORY}/common/mocks/mock_gmm_page_table_mngr.cpp
    ${NEO_SHARED_TEST_DIRECTORY}/common/test_macros/test_excludes.cpp
    ${NEO_SHARED_TEST_DIRECTORY}/common/test_macros/test_excludes.h
    ${NEO_SHARED_TEST_DIRECTORY}/unit_test/base_ult_config_listener.cpp
    ${NEO_SHARED_TEST_DIRECTORY}/unit_test/base_ult_config_listener.h
    ${NEO_SHARED_TEST_DIRECTORY}/unit_test/helpers/debug_helpers.cpp
    ${NEO_SHARED_TEST_DIRECTORY}/unit_test/utilities/cpuintrinsics.cpp
    ${NEO_SHARED_DIRECTORY}/gmm_helper/resource_info_impl.cpp
)

get_property(NEO_SHARED_TESTS_CMD_PARSE GLOBAL PROPERTY NEO_SHARED_TESTS_CMD_PARSE)

list(APPEND neo_libult_common_SRCS_LIB_ULT
     ${NEO_SHARED_TESTS_CMD_PARSE}
)

if(UNIX)
  list(APPEND neo_libult_common_SRCS_ENABLE_TESTED_HW ${neo_libult_common_SRCS_ENABLE_TESTED_HW_LINUX})
endif()

add_library(neo_libult_common OBJECT EXCLUDE_FROM_ALL
            ${neo_libult_common_SRCS_LIB_ULT}
            ${neo_libult_common_SRCS_ENABLE_TESTED_HW}
)

set(neo_libult_common_SRCS_LIB_ULT_WIN
    ${NEO_SHARED_TEST_DIRECTORY}/common/os_interface/windows/ult_dxcore_factory.cpp
    ${NEO_SHARED_TEST_DIRECTORY}/common/os_interface/windows/ult_dxcore_factory.h
    ${NEO_SHARED_TEST_DIRECTORY}/common/os_interface/windows/ult_dxgi_factory.cpp
    ${NEO_SHARED_TEST_DIRECTORY}/common/os_interface/windows/ult_dxgi_factory.h
)

set(neo_libult_common_SRCS_LIB_ULT_LINUX
    ${NEO_SHARED_TEST_DIRECTORY}/common/os_interface/linux/device_command_stream_fixture.cpp
    ${NEO_SHARED_TEST_DIRECTORY}/common/os_interface/linux/device_command_stream_fixture.h
)
set_property(GLOBAL PROPERTY neo_libult_common_SRCS_LIB_ULT_LINUX ${neo_libult_common_SRCS_LIB_ULT_LINUX})

if(UNIX AND NOT DISABLE_WDDM_LINUX)
  target_sources(neo_libult_common PRIVATE
                 ${NEO_SHARED_DIRECTORY}/os_interface/windows/wddm/wddm_create.cpp
  )
endif()
if(WIN32)
  target_sources(neo_libult_common PRIVATE ${neo_libult_common_SRCS_LIB_ULT_WIN})
elseif(UNIX)
  target_sources(neo_libult_common PRIVATE ${neo_libult_common_SRCS_LIB_ULT_LINUX})
endif()

set(neo_libult_SRCS_LINUX
    ${NEO_SHARED_TEST_DIRECTORY}/common/libult/os_interface.cpp
    ${NEO_SHARED_TEST_DIRECTORY}/common/mocks/linux/mock_drm_memory_manager.cpp
    ${NEO_SHARED_TEST_DIRECTORY}/common/mocks/linux/mock_drm_memory_manager.h
    ${NEO_SHARED_TEST_DIRECTORY}/common/mocks/mock_direct_submission_controller_enabled.cpp
    ${NEO_SHARED_TEST_DIRECTORY}/common/os_interface/linux/create_drm_memory_manager.cpp
    ${NEO_SHARED_TEST_DIRECTORY}/common/os_interface/linux/drm_neo_create.cpp
    ${NEO_SHARED_TEST_DIRECTORY}/common/os_interface/linux/options.cpp
    ${NEO_SHARED_TEST_DIRECTORY}/common/os_interface/linux/sys_calls_linux_ult.cpp
)

set_property(GLOBAL APPEND PROPERTY neo_libult_SRCS_LINUX ${neo_libult_SRCS_LINUX})
set(neo_libult_SRCS_WINDOWS
    ${NEO_SHARED_TEST_DIRECTORY}/common/libult/os_interface.cpp
    ${NEO_SHARED_TEST_DIRECTORY}/common/mocks/mock_direct_submission_controller_enabled.cpp
    ${NEO_SHARED_TEST_DIRECTORY}/common/mocks/mock_wddm.cpp
    ${NEO_SHARED_TEST_DIRECTORY}/common/os_interface/windows/create_wddm_memory_manager.cpp
    ${NEO_SHARED_TEST_DIRECTORY}/common/os_interface/windows/options.cpp
    ${NEO_SHARED_TEST_DIRECTORY}/common/os_interface/windows/sys_calls.cpp
    ${NEO_SHARED_TEST_DIRECTORY}/common/os_interface/windows/wddm_calls.cpp
    ${NEO_SHARED_TEST_DIRECTORY}/common/os_interface/windows/wddm_create.cpp
    ${NEO_SHARED_TEST_DIRECTORY}/common/os_interface/windows/mock_environment_variables.cpp
    ${NEO_SHARED_TEST_DIRECTORY}/common/os_interface/windows/mock_environment_variables.h
)

set_property(GLOBAL APPEND PROPERTY neo_libult_SRCS_WINDOWS ${neo_libult_SRCS_WINDOWS})
if(UNIX)
  set(neo_libult_SRCS ${neo_libult_SRCS_LINUX})
else()
  set(neo_libult_SRCS ${neo_libult_SRCS_WINDOWS})
endif()

add_library(neo_libult OBJECT EXCLUDE_FROM_ALL ${neo_libult_SRCS})

foreach(target_name "neo_libult_common" "neo_libult")
  set_target_properties(${target_name} PROPERTIES POSITION_INDEPENDENT_CODE ON)
  set_target_properties(${target_name} PROPERTIES FOLDER ${SHARED_TEST_PROJECTS_FOLDER})
  set_property(TARGET ${target_name} APPEND_STRING PROPERTY COMPILE_FLAGS ${ASAN_FLAGS} ${TSAN_FLAGS})
  target_include_directories(${target_name} PRIVATE
                             $<TARGET_PROPERTY:${NEO_SHARED_MOCKABLE_LIB_NAME},INTERFACE_INCLUDE_DIRECTORIES>
                             $<TARGET_PROPERTY:gmock-gtest,INTERFACE_INCLUDE_DIRECTORIES>
                             ${SOURCE_LEVEL_DEBUGGER_HEADERS_DIR}
  )
  target_compile_definitions(${target_name} PRIVATE $<TARGET_PROPERTY:${NEO_SHARED_MOCKABLE_LIB_NAME},INTERFACE_COMPILE_DEFINITIONS>)
  create_project_source_tree(${target_name})
endforeach()

add_subdirectories()
