#
# Copyright (C) 2018-2025 Intel Corporation
#
# SPDX-License-Identifier: MIT
#

project(ocloc_tests)

set(IGDRCL_SRCS_cloc
    ${OCLOC_DIRECTORY}/source/decoder/binary_decoder.cpp
    ${OCLOC_DIRECTORY}/source/decoder/binary_encoder.cpp
    ${OCLOC_DIRECTORY}/source/offline_compiler.cpp
    ${OCLOC_DIRECTORY}/source/offline_linker.cpp
    ${OCLOC_DIRECTORY}/source/ocloc_concat.cpp
    ${OCLOC_DIRECTORY}/source/ocloc_fatbinary.cpp
    ${OCLOC_DIRECTORY}/source/ocloc_supported_devices_helper.h
    ${OCLOC_DIRECTORY}/source/ocloc_supported_devices_helper.cpp
)

if(WIN32)
  list(APPEND IGDRCL_SRCS_cloc
       ${OCLOC_DIRECTORY}/source/windows/ocloc_supported_devices_helper_windows.cpp
  )
else()
  list(APPEND IGDRCL_SRCS_cloc
       ${OCLOC_DIRECTORY}/source/linux/ocloc_supported_devices_helper_linux.cpp
  )
endif()

set(IGDRCL_SRCS_offline_compiler_mock
    ${CMAKE_CURRENT_SOURCE_DIR}/decoder/mock/mock_decoder.h
    ${CMAKE_CURRENT_SOURCE_DIR}/decoder/mock/mock_encoder.h
    ${CMAKE_CURRENT_SOURCE_DIR}/decoder/mock/mock_zebin_decoder.h
    ${CMAKE_CURRENT_SOURCE_DIR}/decoder/mock/mock_zebin_encoder.h
    ${CMAKE_CURRENT_SOURCE_DIR}/decoder/mock/mock_iga_wrapper.h
)

set(CLOC_LIB_SRCS_UTILITIES
    ${OCLOC_DIRECTORY}/source/utilities/safety_caller.h
    ${OCLOC_DIRECTORY}/source/utilities/get_current_dir.h
)

if(WIN32)
  list(APPEND CLOC_LIB_SRCS_UTILITIES
       ${OCLOC_DIRECTORY}/source/utilities/windows/safety_caller_windows.cpp
       ${OCLOC_DIRECTORY}/source/utilities/windows/safety_guard_windows.h
       ${OCLOC_DIRECTORY}/source/utilities/windows/seh_exception.cpp
       ${OCLOC_DIRECTORY}/source/utilities/windows/seh_exception.h
       ${OCLOC_DIRECTORY}/source/utilities/windows/get_current_dir_windows.cpp
  )
else()
  list(APPEND CLOC_LIB_SRCS_UTILITIES
       ${OCLOC_DIRECTORY}/source/utilities/linux/safety_caller_linux.cpp
       ${OCLOC_DIRECTORY}/source/utilities/linux/safety_guard_linux.h
       ${OCLOC_DIRECTORY}/source/utilities/linux/get_current_dir_linux.cpp
  )
endif()

get_property(NEO_CORE_tests_compiler_mocks GLOBAL PROPERTY NEO_CORE_tests_compiler_mocks)

set(IGDRCL_SRCS_offline_compiler_tests
    ${CMAKE_CURRENT_SOURCE_DIR}/CMakeLists.txt
    ${CMAKE_CURRENT_SOURCE_DIR}/decoder/decoder_tests.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/decoder/encoder_tests.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/decoder/iga_wrapper_tests.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/decoder/iga_wrapper_tests.h
    ${CMAKE_CURRENT_SOURCE_DIR}/decoder/zebin_manipulator_tests.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/default_cache_config_tests.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/environment.h
    ${CMAKE_CURRENT_SOURCE_DIR}/main.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/ocloc_api_tests.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/ocloc_concat_tests.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/ocloc_fatbinary_tests.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/ocloc_fatbinary_tests.h
    ${CMAKE_CURRENT_SOURCE_DIR}/ocloc_fcl_facade_tests.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/ocloc_fcl_facade_tests.h
    ${CMAKE_CURRENT_SOURCE_DIR}/ocloc_igc_facade_tests.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/ocloc_igc_facade_tests.h
    ${CMAKE_CURRENT_SOURCE_DIR}/ocloc_product_config_tests.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/ocloc_product_config_tests.h
    ${CMAKE_CURRENT_SOURCE_DIR}/ocloc_supported_devices_helper_tests.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/ocloc_tests_configuration.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/ocloc_validator_tests.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/offline_compiler_tests.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/offline_compiler_tests.h
    ${CMAKE_CURRENT_SOURCE_DIR}${BRANCH_DIR_SUFFIX}/offline_compiler_ext_tests.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/offline_linker_tests.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/offline_linker_tests.h
    ${CMAKE_CURRENT_SOURCE_DIR}/stdout_capturer.h
    ${NEO_SHARED_DIRECTORY}/helpers/abort.cpp
    ${NEO_SHARED_DIRECTORY}/helpers/file_io_load.cpp
    ${NEO_SHARED_DIRECTORY}/memory_manager/deferred_deleter.cpp
    ${NEO_SHARED_DIRECTORY}/memory_manager/deferred_deleter.h
    ${NEO_SHARED_TEST_DIRECTORY}/common/mocks/mock_compilers.cpp
    ${NEO_SHARED_TEST_DIRECTORY}/common/mocks${BRANCH_DIR_SUFFIX}/mock_igc_path.cpp
    ${NEO_SHARED_TEST_DIRECTORY}/common/mocks/mock_os_library.cpp
    ${NEO_SHARED_TEST_DIRECTORY}/common/mocks/mock_os_library.h
    ${NEO_SHARED_TEST_DIRECTORY}/common/mocks/mock_compilers.h
    ${NEO_SHARED_TEST_DIRECTORY}/common/mocks/mock_modules_zebin.cpp
    ${NEO_SHARED_TEST_DIRECTORY}/common/mocks/mock_modules_zebin.h
    ${NEO_SHARED_TEST_DIRECTORY}/common/helpers/memory_leak_listener.cpp
    ${NEO_SHARED_TEST_DIRECTORY}/common/helpers/memory_leak_listener.h
    ${NEO_SHARED_TEST_DIRECTORY}/common/helpers/memory_management.cpp
    ${NEO_SHARED_TEST_DIRECTORY}/common/helpers/memory_management.h
    ${NEO_SHARED_TEST_DIRECTORY}/common/helpers/file_io.cpp
    ${NEO_SHARED_TEST_DIRECTORY}/common/helpers/test_files.cpp
    ${NEO_SHARED_TEST_DIRECTORY}/common/libult/signal_utils.h
    ${NEO_SHARED_TEST_DIRECTORY}/common/libult/create_directory.cpp
    ${NEO_SHARED_TEST_DIRECTORY}/common/libult/debug_settings_reader_creator.cpp
    ${NEO_SHARED_TEST_DIRECTORY}/common/libult/mock_io_functions.cpp
    ${NEO_SHARED_TEST_DIRECTORY}/common/test_macros/test_excludes.cpp
    ${NEO_SHARED_TEST_DIRECTORY}/common/helpers/virtual_file_system_listener.cpp
    ${IGDRCL_SRCS_cloc}
    ${IGDRCL_SRCS_offline_compiler_mock}
    ${NEO_CORE_tests_compiler_mocks}
    ${CLOC_LIB_SRCS_LIB}
    ${CLOC_LIB_SRCS_UTILITIES}
)
append_sources_from_properties(IGDRCL_SRCS_offline_compiler_tests NEO_SRCS_ENABLE_CORE)

if(WIN32)
  list(APPEND IGDRCL_SRCS_offline_compiler_tests
       ${NEO_SHARED_DIRECTORY}/os_interface/windows/os_thread_win.cpp
       ${NEO_SHARED_TEST_DIRECTORY}/common/os_interface/windows/signal_utils.cpp
       ${CMAKE_CURRENT_SOURCE_DIR}/windows/ocloc_supported_devices_helper_windows_tests.cpp
  )
  list(REMOVE_ITEM IGDRCL_SRCS_offline_compiler_tests
       ${NEO_SHARED_DIRECTORY}/utilities/windows/directory.cpp
  )
else()
  list(APPEND IGDRCL_SRCS_offline_compiler_tests
       ${NEO_SHARED_DIRECTORY}/os_interface/linux/os_library_helper.cpp
       ${NEO_SHARED_DIRECTORY}/os_interface/linux/os_thread_linux.cpp
       ${NEO_SHARED_DIRECTORY}/os_interface/linux/sys_calls_linux.cpp
       ${NEO_SHARED_TEST_DIRECTORY}/common/os_interface/linux/signal_utils.cpp
       ${CMAKE_CURRENT_SOURCE_DIR}/linux/ocloc_supported_devices_helper_linux_tests.cpp
  )
  list(REMOVE_ITEM IGDRCL_SRCS_offline_compiler_tests
       ${NEO_SHARED_DIRECTORY}/utilities/linux/directory.cpp
  )
endif()

link_directories(${CMAKE_RUNTIME_OUTPUT_DIRECTORY})

add_executable(ocloc_tests ${IGDRCL_SRCS_offline_compiler_tests})

target_include_directories(ocloc_tests PRIVATE
                           ${NEO_SHARED_TEST_DIRECTORY}/common/test_macros/header${BRANCH_DIR_SUFFIX}
                           ${NEO_SHARED_TEST_DIRECTORY}/common/test_configuration/unit_tests
                           $<TARGET_PROPERTY:ocloc_lib,INCLUDE_DIRECTORIES>
)

string(TOLOWER ${DEFAULT_TESTED_PLATFORM} CLOC_LIB_DEFAULT_DEVICE)
target_compile_definitions(ocloc_tests PUBLIC
                           MOCKABLE_VIRTUAL=virtual
                           $<TARGET_PROPERTY:ocloc_lib,INTERFACE_COMPILE_DEFINITIONS>
                           ${TESTED_CORE_FLAGS_DEFINITONS}
                           NEO_OCLOC_CURRENT_LIB_NAME="ocloc-current"
                           NEO_OCLOC_FORMER_LIB_NAME="ocloc-former"
                           DEFAULT_TEST_PLATFORM_NAME="${CLOC_LIB_DEFAULT_DEVICE}"
)

target_link_libraries(ocloc_tests gmock-gtest)

if(WIN32)
  target_link_libraries(ocloc_tests dbghelp)
endif()

if(UNIX)
  target_link_libraries(ocloc_tests dl pthread)
endif()

get_property(CLOC_LIB_FOLDER TARGET ocloc_lib PROPERTY FOLDER)
get_property(CLOC_LIB_COMPILE_FLAGS TARGET ocloc_lib PROPERTY COMPILE_FLAGS)
set_property(TARGET ocloc_tests PROPERTY FOLDER ${CLOC_LIB_FOLDER})
set_property(TARGET ocloc_tests APPEND_STRING PROPERTY COMPILE_FLAGS ${CLOC_LIB_COMPILE_FLAGS} ${ASAN_FLAGS} ${TSAN_FLAGS})
set_property(TARGET ocloc_tests PROPERTY ENABLE_EXPORTS TRUE)

add_custom_target(run_ocloc_tests ALL
                  DEPENDS ocloc_tests
)
if(${NEO_TARGET_PROCESSOR} STREQUAL "x86_64")
  add_dependencies(run_unit_tests run_ocloc_tests)
  add_dependencies(unit_tests ocloc_tests)
endif()

macro(macro_for_each_platform)
  if("${PLATFORM_IT_LOWER}" STREQUAL "${CLOC_LIB_DEFAULT_DEVICE}")
    list(GET ${PLATFORM_IT}_${CORE_TYPE}_REVISIONS 0 REVISION_CONFIG)
    parse_revision_config(${REVISION_CONFIG} ${PLATFORM_IT_LOWER} DEVICE_ID REVISION_ID)
    add_dependencies(run_ocloc_tests prepare_test_kernels_${PLATFORM_IT_LOWER}_${REVISION_ID})
    add_dependencies(unit_tests prepare_test_kernels_${PLATFORM_IT_LOWER}_${REVISION_ID})
    set(run_tests_cmd ocloc_tests --device ${PLATFORM_IT_LOWER})
  endif()
endmacro()
macro(macro_for_each_core_type)
  apply_macro_for_each_platform("TESTED")
endmacro()
apply_macro_for_each_core_type("TESTED")

set_property(TARGET run_ocloc_tests PROPERTY FOLDER ${CLOC_LIB_FOLDER})

if(WIN32)
  add_custom_command(
                     TARGET run_ocloc_tests
                     POST_BUILD
                     COMMAND echo deleting offline compiler files and directories...
                     COMMAND ${CMAKE_COMMAND} -E remove ${TargetDir}/${CLOC_LIB_DEFAULT_DEVICE}/copybuffer_${CLOC_LIB_DEFAULT_DEVICE}.spv
                     COMMAND ${CMAKE_COMMAND} -E remove ${TargetDir}/${CLOC_LIB_DEFAULT_DEVICE}/copybuffer_${CLOC_LIB_DEFAULT_DEVICE}.gen
                     COMMAND ${CMAKE_COMMAND} -E remove ${TargetDir}/${CLOC_LIB_DEFAULT_DEVICE}/copybuffer_${CLOC_LIB_DEFAULT_DEVICE}.bin
                     COMMAND ${CMAKE_COMMAND} -E remove_directory ${TargetDir}/${CLOC_LIB_DEFAULT_DEVICE}/offline_compiler_test
                     COMMAND ${run_tests_cmd}
                     WORKING_DIRECTORY ${TargetDir}
  )
else()
  add_custom_command(
                     TARGET run_ocloc_tests
                     POST_BUILD
                     COMMAND echo deleting offline compiler files and directories...
                     COMMAND ${CMAKE_COMMAND} -E remove ${TargetDir}/${CLOC_LIB_DEFAULT_DEVICE}/*.spv
                     COMMAND ${CMAKE_COMMAND} -E remove ${TargetDir}/${CLOC_LIB_DEFAULT_DEVICE}/*.gen
                     COMMAND ${CMAKE_COMMAND} -E remove ${TargetDir}/${CLOC_LIB_DEFAULT_DEVICE}/*.ll
                     COMMAND ${CMAKE_COMMAND} -E remove ${TargetDir}/${CLOC_LIB_DEFAULT_DEVICE}/*.bin
                     COMMAND ${CMAKE_COMMAND} -E remove_directory ${TargetDir}/${CLOC_LIB_DEFAULT_DEVICE}/offline_compiler_test
                     COMMAND ${run_tests_cmd}
                     WORKING_DIRECTORY ${TargetDir}
  )
endif()

add_subdirectories()
create_project_source_tree(ocloc_tests)
