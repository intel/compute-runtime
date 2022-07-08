#
# Copyright (C) 2018-2022 Intel Corporation
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
)

set(IGDRCL_SRCS_offline_compiler_mock
    ${CMAKE_CURRENT_SOURCE_DIR}/decoder/mock/mock_decoder.h
    ${CMAKE_CURRENT_SOURCE_DIR}/decoder/mock/mock_encoder.h
    ${CMAKE_CURRENT_SOURCE_DIR}/decoder/mock/mock_iga_wrapper.h
    ${CMAKE_CURRENT_SOURCE_DIR}/mock/mock_argument_helper.h
    ${CMAKE_CURRENT_SOURCE_DIR}/mock/mock_multi_command.h
    ${CMAKE_CURRENT_SOURCE_DIR}/mock/mock_ocloc_concat.h
    ${CMAKE_CURRENT_SOURCE_DIR}/mock/mock_ocloc_fcl_facade.h
    ${CMAKE_CURRENT_SOURCE_DIR}/mock/mock_ocloc_igc_facade.h
    ${CMAKE_CURRENT_SOURCE_DIR}/mock/mock_offline_compiler.h
    ${CMAKE_CURRENT_SOURCE_DIR}/mock/mock_offline_linker.h
    ${CMAKE_CURRENT_SOURCE_DIR}/mock/mock_sip_ocloc_tests.cpp
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
    ${CMAKE_CURRENT_SOURCE_DIR}/environment.h
    ${CMAKE_CURRENT_SOURCE_DIR}/main.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/mock/mock_iga_dll.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/mock/mock_iga_dll.h
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
    ${CMAKE_CURRENT_SOURCE_DIR}/ocloc_tests_configuration.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/ocloc_validator_tests.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/offline_compiler_tests.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/offline_compiler_tests.h
    ${CMAKE_CURRENT_SOURCE_DIR}/offline_linker_tests.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/offline_linker_tests.h
    ${CMAKE_CURRENT_SOURCE_DIR}/stdout_capturer.h
    ${NEO_SHARED_DIRECTORY}/helpers/abort.cpp
    ${NEO_SHARED_DIRECTORY}/helpers/file_io.cpp
    ${NEO_SHARED_DIRECTORY}/memory_manager/deferred_deleter.cpp
    ${NEO_SHARED_DIRECTORY}/memory_manager/deferred_deleter.h
    ${NEO_SHARED_TEST_DIRECTORY}/common/mocks/mock_compilers.cpp
    ${NEO_SHARED_TEST_DIRECTORY}/common/mocks/mock_compilers.h
    ${NEO_SHARED_TEST_DIRECTORY}/unit_test/device_binary_format/zebin_tests.h
    ${NEO_SHARED_TEST_DIRECTORY}/common/helpers/test_files.cpp
    ${NEO_SHARED_TEST_DIRECTORY}/common/libult/signal_utils.h
    ${NEO_SHARED_TEST_DIRECTORY}/common/test_macros/test_excludes.cpp
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
  )
else()
  list(APPEND IGDRCL_SRCS_offline_compiler_tests
       ${NEO_SHARED_DIRECTORY}/os_interface/linux/os_thread_linux.cpp
       ${NEO_SHARED_DIRECTORY}/os_interface/linux/sys_calls_linux.cpp
       ${NEO_SHARED_TEST_DIRECTORY}/common/os_interface/linux/signal_utils.cpp
       ${OCLOC_DIRECTORY}/source/linux/os_library_ocloc_helper.cpp
  )
endif()

link_directories(${CMAKE_RUNTIME_OUTPUT_DIRECTORY})

add_executable(ocloc_tests ${IGDRCL_SRCS_offline_compiler_tests})

target_include_directories(ocloc_tests PRIVATE
                           ${NEO_SHARED_TEST_DIRECTORY}/common/test_macros/header${BRANCH_DIR_SUFFIX}
                           ${NEO_SHARED_TEST_DIRECTORY}/common/test_configuration/unit_tests
                           $<TARGET_PROPERTY:ocloc_lib,INCLUDE_DIRECTORIES>
)
target_compile_definitions(ocloc_tests PUBLIC MOCKABLE_VIRTUAL=virtual $<TARGET_PROPERTY:ocloc_lib,INTERFACE_COMPILE_DEFINITIONS> ${TESTED_CORE_FLAGS_DEFINITONS})

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

string(TOLOWER ${DEFAULT_TESTED_PLATFORM} CLOC_LIB_DEFAULT_DEVICE)

add_custom_target(run_ocloc_tests ALL
                  DEPENDS ocloc_tests
)
if(${NEO_TARGET_PROCESSOR} STREQUAL "x86_64")
  add_dependencies(run_unit_tests run_ocloc_tests)
  add_dependencies(unit_tests ocloc_tests)
endif()

macro(macro_for_each_platform)
  if("${PLATFORM_IT_LOWER}" STREQUAL "${CLOC_LIB_DEFAULT_DEVICE}")
    foreach(PLATFORM_TYPE ${PLATFORM_TYPES})
      if(${PLATFORM_IT}_IS_${PLATFORM_TYPE})
        list(GET ${PLATFORM_TYPE}_${CORE_TYPE}_REVISIONS 0 REVISION_ID)
        get_family_name_with_type(${CORE_TYPE} ${PLATFORM_TYPE})
        add_dependencies(run_ocloc_tests prepare_test_kernels_${family_name_with_type}_${REVISION_ID})
        add_dependencies(unit_tests prepare_test_kernels_${family_name_with_type}_${REVISION_ID})
        set(run_tests_cmd ocloc_tests --device ${CLOC_LIB_DEFAULT_DEVICE} --family_type ${family_name_with_type} --rev_id ${REVISION_ID})
      endif()
    endforeach()
  endif()
endmacro()
macro(macro_for_each_core_type)
  apply_macro_for_each_platform()
endmacro()
apply_macro_for_each_core_type("TESTED")

set_property(TARGET run_ocloc_tests PROPERTY FOLDER ${CLOC_LIB_FOLDER})

if(WIN32)
  add_custom_command(
                     TARGET run_ocloc_tests
                     POST_BUILD
                     COMMAND echo deleting offline compiler files and directories...
                     COMMAND ${CMAKE_COMMAND} -E remove ${TargetDir}/${CLOC_LIB_DEFAULT_DEVICE}/copybuffer_${CLOC_LIB_DEFAULT_DEVICE}.bc
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
                     COMMAND ${CMAKE_COMMAND} -E remove ${TargetDir}/${CLOC_LIB_DEFAULT_DEVICE}/*.bc
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
