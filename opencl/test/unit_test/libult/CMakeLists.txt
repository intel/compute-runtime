#
# Copyright (C) 2018-2020 Intel Corporation
#
# SPDX-License-Identifier: MIT
#

macro(macro_for_each_gen)
  list(APPEND IGDRCL_SRCS_ENABLE_TESTED_HW
    ${NEO_SHARED_TEST_DIRECTORY}/unit_test/${GEN_TYPE_LOWER}/cmd_parse_${GEN_TYPE_LOWER}.cpp
    ${NEO_SHARED_TEST_DIRECTORY}/unit_test/libult/${GEN_TYPE_LOWER}.cpp
    ${NEO_SHARED_DIRECTORY}/${GEN_TYPE_LOWER}/enable_hw_info_config_${GEN_TYPE_LOWER}.cpp
    ${NEO_SHARED_DIRECTORY}/${GEN_TYPE_LOWER}/enable_${GEN_TYPE_LOWER}.cpp
    ${NEO_SHARED_DIRECTORY}/${GEN_TYPE_LOWER}/enable_family_full_core_${GEN_TYPE_LOWER}.cpp
    ${NEO_SOURCE_DIR}/opencl/source/${GEN_TYPE_LOWER}/enable_family_full_ocl_${GEN_TYPE_LOWER}.cpp
  )

  if(EXISTS "${NEO_SHARED_TEST_DIRECTORY}/unit_test/${GEN_TYPE_LOWER}/cmd_parse${BRANCH_DIR_SUFFIX}/cmd_parse_${GEN_TYPE_LOWER}.inl")
    list(APPEND IGDRCL_SRCS_ENABLE_TESTED_HW "${NEO_SHARED_TEST_DIRECTORY}/unit_test/${GEN_TYPE_LOWER}/cmd_parse${BRANCH_DIR_SUFFIX}/cmd_parse_${GEN_TYPE_LOWER}.inl")
    include_directories(${NEO_SHARED_TEST_DIRECTORY}/unit_test/${GEN_TYPE_LOWER}/cmd_parse${BRANCH_DIR_SUFFIX}/)
  endif()
endmacro()

apply_macro_for_each_gen("TESTED")

set(IGDRCL_SRCS_LIB_ULT
  ${NEO_SHARED_DIRECTORY}/helpers/allow_deferred_deleter.cpp
  ${NEO_SHARED_TEST_DIRECTORY}/unit_test/utilities/cpuintrinsics.cpp
  ${NEO_SOURCE_DIR}/opencl/source/compiler_interface/default_cache_config.cpp
  ${NEO_SOURCE_DIR}/opencl/source/dll/debugger.cpp
  ${NEO_SOURCE_DIR}/opencl/test/unit_test/abort.cpp
  ${NEO_SOURCE_DIR}/opencl/test/unit_test/helpers/built_ins_helper.cpp
  ${NEO_SOURCE_DIR}/opencl/test/unit_test/helpers/debug_helpers.cpp
  ${NEO_SOURCE_DIR}/opencl/test/unit_test/helpers/execution_environment_helper.cpp
  ${NEO_SOURCE_DIR}/opencl/test/unit_test/helpers/execution_environment_helper.h
  ${NEO_SOURCE_DIR}/opencl/test/unit_test/helpers/test_files.cpp
  ${NEO_SOURCE_DIR}/opencl/test/unit_test/helpers/test_files.h
  ${NEO_SOURCE_DIR}/opencl/test/unit_test/libult/create_tbx_sockets.cpp
  ${NEO_SOURCE_DIR}/opencl/test/unit_test/libult/debug_manager.cpp
  ${NEO_SOURCE_DIR}/opencl/test/unit_test/libult/source_level_debugger.cpp
  ${NEO_SOURCE_DIR}/opencl/test/unit_test/libult/source_level_debugger_library.cpp
  ${NEO_SOURCE_DIR}/opencl/test/unit_test/libult/source_level_debugger_library.h
  ${NEO_SOURCE_DIR}/opencl/test/unit_test/libult/ult_aub_command_stream_receiver.h
  ${NEO_SOURCE_DIR}/opencl/test/unit_test/libult/ult_command_stream_receiver.h
  ${NEO_SOURCE_DIR}/opencl/test/unit_test/utilities/debug_settings_reader_creator.cpp
  ${NEO_SHARED_TEST_DIRECTORY}/unit_test/helpers/memory_leak_listener.cpp
  ${NEO_SHARED_TEST_DIRECTORY}/unit_test/helpers/memory_leak_listener.h
  ${NEO_SHARED_TEST_DIRECTORY}/unit_test/helpers/memory_management.cpp
  ${NEO_SHARED_TEST_DIRECTORY}/unit_test/helpers/memory_management.h
)

get_property(NEO_SHARED_TESTS_CMD_PARSE GLOBAL PROPERTY NEO_SHARED_TESTS_CMD_PARSE)

list(APPEND IGDRCL_SRCS_LIB_ULT
   ${NEO_SHARED_TESTS_CMD_PARSE}
)

set(IGDRCL_SRCS_LIB_ULT_WINDOWS
  ${NEO_SOURCE_DIR}/opencl/test/unit_test/os_interface/windows/mock_environment_variables.cpp
)

add_library (igdrcl_libult OBJECT EXCLUDE_FROM_ALL
  ${IGDRCL_SRCS_LIB_ULT}
  ${IGDRCL_SRCS_ENABLE_TESTED_HW}
)
if(UNIX)
  target_sources(igdrcl_libult PRIVATE ${IGDRCL_SRCS_ENABLE_TESTED_HW_LINUX})
endif()

set(IGDRCL_SRCS_LIB_ULT_CS
  ${NEO_SOURCE_DIR}/opencl/test/unit_test/libult/create_command_stream.cpp
  ${NEO_SOURCE_DIR}/opencl/test/unit_test/libult/create_command_stream.h
)
add_library (igdrcl_libult_cs OBJECT EXCLUDE_FROM_ALL
  ${IGDRCL_SRCS_LIB_ULT_CS}
)
target_include_directories(igdrcl_libult PRIVATE
  $<TARGET_PROPERTY:gmock-gtest,INTERFACE_INCLUDE_DIRECTORIES>
)

set(IGDRCL_SRCS_LIB_ULT_ENV
  ${NEO_SOURCE_DIR}/opencl/test/unit_test/custom_event_listener.h
  ${NEO_SOURCE_DIR}/opencl/test/unit_test/global_environment.cpp
  ${NEO_SOURCE_DIR}/opencl/test/unit_test/global_environment.h
  ${NEO_SOURCE_DIR}/opencl/test/unit_test/main.cpp
  ${NEO_SOURCE_DIR}/opencl/test/unit_test/options_unit_tests.cpp
  ${NEO_SOURCE_DIR}/opencl/test/unit_test/command_queue/command_queue_fixture.cpp
  ${NEO_SOURCE_DIR}/opencl/test/unit_test/command_queue/command_queue_fixture.h
  ${NEO_SOURCE_DIR}/opencl/test/unit_test/fixtures/memory_management_fixture.cpp
  ${NEO_SOURCE_DIR}/opencl/test/unit_test/fixtures/memory_management_fixture.h
  ${NEO_SOURCE_DIR}/opencl/test/unit_test/fixtures/built_in_fixture.cpp
  ${NEO_SOURCE_DIR}/opencl/test/unit_test/fixtures/built_in_fixture.h
  ${NEO_SOURCE_DIR}/opencl/test/unit_test/fixtures/buffer_fixture.cpp
  ${NEO_SOURCE_DIR}/opencl/test/unit_test/fixtures/buffer_fixture.h
  ${NEO_SOURCE_DIR}/opencl/test/unit_test/fixtures/device_fixture.cpp
  ${NEO_SOURCE_DIR}/opencl/test/unit_test/fixtures/device_fixture.h
  ${NEO_SOURCE_DIR}/opencl/test/unit_test/fixtures/program_fixture.cpp
  ${NEO_SOURCE_DIR}/opencl/test/unit_test/fixtures/program_fixture.h
  ${NEO_SOURCE_DIR}/opencl/test/unit_test/helpers/kernel_binary_helper.cpp
  ${NEO_SOURCE_DIR}/opencl/test/unit_test/helpers/kernel_binary_helper.h
  ${NEO_SOURCE_DIR}/opencl/test/unit_test/helpers${BRANCH_DIR_SUFFIX}/kernel_binary_helper_hash_value.cpp
  ${NEO_SOURCE_DIR}/opencl/test/unit_test/indirect_heap/indirect_heap_fixture.cpp
  ${NEO_SOURCE_DIR}/opencl/test/unit_test/indirect_heap/indirect_heap_fixture.h
  ${NEO_SOURCE_DIR}/opencl/test/unit_test/ult_config_listener.cpp
  ${NEO_SOURCE_DIR}/opencl/test/unit_test/ult_config_listener.h
)

add_library (igdrcl_libult_env OBJECT EXCLUDE_FROM_ALL
  ${IGDRCL_SRCS_LIB_ULT_ENV}
)

set(IGDRCL_SRCS_LIB_ULT_ENV_WINDOWS
  ${NEO_SOURCE_DIR}/opencl/test/unit_test/os_interface/windows/create_wddm_memory_manager.cpp
  ${NEO_SOURCE_DIR}/opencl/test/unit_test/os_interface/windows/options.cpp
  ${NEO_SOURCE_DIR}/opencl/test/unit_test/os_interface/windows/sys_calls.cpp
  ${NEO_SOURCE_DIR}/opencl/test/unit_test/os_interface/windows/ult_dxgi_factory.cpp
  ${NEO_SOURCE_DIR}/opencl/test/unit_test/os_interface/windows/ult_dxgi_factory.h
  ${NEO_SOURCE_DIR}/opencl/test/unit_test/os_interface/windows/wddm_calls.cpp
)

set(IGDRCL_SRCS_LIB_ULT_ENV_LINUX
  ${NEO_SOURCE_DIR}/opencl/test/unit_test/os_interface/linux/allocator_helper.cpp
  ${NEO_SOURCE_DIR}/opencl/test/unit_test/os_interface/linux/create_drm_memory_manager.cpp
  ${NEO_SOURCE_DIR}/opencl/test/unit_test/os_interface/linux/options.cpp
  ${NEO_SOURCE_DIR}/opencl/test/unit_test/os_interface/linux/sys_calls_linux_ult.cpp
)

if(WIN32)
  target_sources(igdrcl_libult_env PRIVATE ${IGDRCL_SRCS_LIB_ULT_ENV_WINDOWS})
  target_sources(igdrcl_libult PRIVATE ${IGDRCL_SRCS_LIB_ULT_WINDOWS})
else()
  target_sources(igdrcl_libult_env PRIVATE ${IGDRCL_SRCS_LIB_ULT_ENV_LINUX})
endif()

target_include_directories(igdrcl_libult PRIVATE
  ${SOURCE_LEVEL_DEBUGGER_HEADERS_DIR}
)

target_include_directories(igdrcl_libult_env PRIVATE
  $<TARGET_PROPERTY:gmock-gtest,INTERFACE_INCLUDE_DIRECTORIES>
  ${NEO_SOURCE_DIR}/opencl/test/unit_test/mocks${BRANCH_DIR_SUFFIX}
)

set_property(GLOBAL PROPERTY IGDRCL_SRCS_ENABLE_TESTED_HW ${IGDRCL_SRCS_ENABLE_TESTED_HW})
set_property(GLOBAL PROPERTY IGDRCL_SRCS_ENABLE_TESTED_HW_LINUX ${IGDRCL_SRCS_ENABLE_TESTED_HW_LINUX})
set_property(GLOBAL PROPERTY IGDRCL_SRCS_LIB_ULT ${IGDRCL_SRCS_LIB_ULT})
set_property(GLOBAL PROPERTY IGDRCL_SRCS_LIB_ULT_CS ${IGDRCL_SRCS_LIB_ULT_CS})
set_property(GLOBAL PROPERTY IGDRCL_SRCS_LIB_ULT_ENV ${IGDRCL_SRCS_LIB_ULT_ENV})
set_property(GLOBAL PROPERTY IGDRCL_SRCS_LIB_ULT_ENV_LINUX ${IGDRCL_SRCS_LIB_ULT_ENV_LINUX})

foreach(target_name igdrcl_libult igdrcl_libult_cs igdrcl_libult_env)
  set_target_properties(${target_name} PROPERTIES POSITION_INDEPENDENT_CODE ON)
  set_target_properties(${target_name} PROPERTIES FOLDER ${OPENCL_TEST_PROJECTS_FOLDER})
  set_property(TARGET ${target_name} APPEND_STRING PROPERTY COMPILE_FLAGS ${ASAN_FLAGS} ${TSAN_FLAGS})
  target_include_directories(${target_name} PRIVATE $<TARGET_PROPERTY:${NEO_MOCKABLE_LIB_NAME},INTERFACE_INCLUDE_DIRECTORIES>)
  target_compile_definitions(${target_name} PRIVATE $<TARGET_PROPERTY:${NEO_MOCKABLE_LIB_NAME},INTERFACE_COMPILE_DEFINITIONS>)
  create_project_source_tree(${target_name})
endforeach()

add_subdirectories()
