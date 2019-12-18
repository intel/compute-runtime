#
# Copyright (C) 2018-2020 Intel Corporation
#
# SPDX-License-Identifier: MIT
#

macro(macro_for_each_gen)
  list(APPEND IGDRCL_SRCS_ENABLE_TESTED_HW
    ${NEO_SOURCE_DIR}/core/unit_tests/${GEN_TYPE_LOWER}/cmd_parse_${GEN_TYPE_LOWER}.cpp
    ${NEO_SOURCE_DIR}/core/unit_tests/libult/${GEN_TYPE_LOWER}.cpp
    ${NEO_SOURCE_DIR}/core/${GEN_TYPE_LOWER}/enable_hw_info_config_${GEN_TYPE_LOWER}.cpp
    ${NEO_SOURCE_DIR}/core/${GEN_TYPE_LOWER}/enable_${GEN_TYPE_LOWER}.cpp
    ${NEO_SOURCE_DIR}/runtime/${GEN_TYPE_LOWER}/enable_family_full_${GEN_TYPE_LOWER}.cpp
  )

  if(EXISTS "${NEO_SOURCE_DIR}/core/unit_tests/${GEN_TYPE_LOWER}/cmd_parse${BRANCH_DIR_SUFFIX}/cmd_parse_${GEN_TYPE_LOWER}.inl")
    list(APPEND IGDRCL_SRCS_ENABLE_TESTED_HW "${NEO_SOURCE_DIR}/core/unit_tests/${GEN_TYPE_LOWER}/cmd_parse${BRANCH_DIR_SUFFIX}/cmd_parse_${GEN_TYPE_LOWER}.inl")
    include_directories(${NEO_SOURCE_DIR}/core/unit_tests/${GEN_TYPE_LOWER}/cmd_parse${BRANCH_DIR_SUFFIX}/)
  endif()
endmacro()

apply_macro_for_each_gen("TESTED")

set(IGDRCL_SRCS_LIB_ULT
  ${NEO_SOURCE_DIR}/core/helpers/allow_deferred_deleter.cpp
  ${NEO_SOURCE_DIR}/core/unit_tests/utilities/clflush.cpp
  ${NEO_SOURCE_DIR}/unit_tests/abort.cpp
  ${NEO_SOURCE_DIR}/unit_tests/helpers/built_ins_helper.cpp
  ${NEO_SOURCE_DIR}/unit_tests/helpers/debug_helpers.cpp
  ${NEO_SOURCE_DIR}/unit_tests/helpers/execution_environment_helper.cpp
  ${NEO_SOURCE_DIR}/unit_tests/helpers/execution_environment_helper.h
  ${NEO_SOURCE_DIR}/unit_tests/helpers/test_files.cpp
  ${NEO_SOURCE_DIR}/unit_tests/helpers/test_files.h
  ${NEO_SOURCE_DIR}/unit_tests/libult/create_tbx_sockets.cpp
  ${NEO_SOURCE_DIR}/unit_tests/libult/debug_manager.cpp
  ${NEO_SOURCE_DIR}/unit_tests/libult/source_level_debugger.cpp
  ${NEO_SOURCE_DIR}/unit_tests/libult/source_level_debugger_library.cpp
  ${NEO_SOURCE_DIR}/unit_tests/libult/source_level_debugger_library.h
  ${NEO_SOURCE_DIR}/unit_tests/libult/ult_aub_command_stream_receiver.h
  ${NEO_SOURCE_DIR}/unit_tests/libult/ult_command_stream_receiver.h
  ${NEO_SOURCE_DIR}/unit_tests/program/evaluate_unhandled_token_ult.cpp
  ${NEO_SOURCE_DIR}/unit_tests/utilities/debug_settings_reader_creator.cpp
  ${NEO_CORE_DIRECTORY}/unit_tests/helpers/memory_leak_listener.cpp
  ${NEO_CORE_DIRECTORY}/unit_tests/helpers/memory_leak_listener.h
  ${NEO_CORE_DIRECTORY}/unit_tests/helpers/memory_management.cpp
  ${NEO_CORE_DIRECTORY}/unit_tests/helpers/memory_management.h
)

set(IGDRCL_SRCS_LIB_ULT_WINDOWS
  ${NEO_SOURCE_DIR}/unit_tests/os_interface/windows/mock_environment_variables.cpp
)

add_library (igdrcl_libult OBJECT EXCLUDE_FROM_ALL
  ${IGDRCL_SRCS_LIB_ULT}
  ${IGDRCL_SRCS_ENABLE_TESTED_HW}
)
if(UNIX)
  target_sources(igdrcl_libult PRIVATE ${IGDRCL_SRCS_ENABLE_TESTED_HW_LINUX})
endif()

set(IGDRCL_SRCS_LIB_ULT_CS
  ${NEO_SOURCE_DIR}/unit_tests/libult/create_command_stream.cpp
  ${NEO_SOURCE_DIR}/unit_tests/libult/create_command_stream.h
)
add_library (igdrcl_libult_cs OBJECT EXCLUDE_FROM_ALL
  ${IGDRCL_SRCS_LIB_ULT_CS}
)
target_include_directories(igdrcl_libult PRIVATE
  $<TARGET_PROPERTY:gmock-gtest,INTERFACE_INCLUDE_DIRECTORIES>
)

set(IGDRCL_SRCS_LIB_ULT_ENV
  ${NEO_SOURCE_DIR}/unit_tests/custom_event_listener.h
  ${NEO_SOURCE_DIR}/unit_tests/global_environment.cpp
  ${NEO_SOURCE_DIR}/unit_tests/global_environment.h
  ${NEO_SOURCE_DIR}/unit_tests/main.cpp
  ${NEO_SOURCE_DIR}/unit_tests/options_unit_tests.cpp
  ${NEO_SOURCE_DIR}/unit_tests/command_queue/command_queue_fixture.cpp
  ${NEO_SOURCE_DIR}/unit_tests/command_queue/command_queue_fixture.h
  ${NEO_SOURCE_DIR}/unit_tests/fixtures/memory_management_fixture.cpp
  ${NEO_SOURCE_DIR}/unit_tests/fixtures/memory_management_fixture.h
  ${NEO_SOURCE_DIR}/unit_tests/fixtures/built_in_fixture.cpp
  ${NEO_SOURCE_DIR}/unit_tests/fixtures/built_in_fixture.h
  ${NEO_SOURCE_DIR}/unit_tests/fixtures/buffer_fixture.cpp
  ${NEO_SOURCE_DIR}/unit_tests/fixtures/buffer_fixture.h
  ${NEO_SOURCE_DIR}/unit_tests/fixtures/device_fixture.cpp
  ${NEO_SOURCE_DIR}/unit_tests/fixtures/device_fixture.h
  ${NEO_SOURCE_DIR}/unit_tests/fixtures/program_fixture.cpp
  ${NEO_SOURCE_DIR}/unit_tests/fixtures/program_fixture.h
  ${NEO_SOURCE_DIR}/unit_tests/helpers/kernel_binary_helper.cpp
  ${NEO_SOURCE_DIR}/unit_tests/helpers/kernel_binary_helper.h
  ${NEO_SOURCE_DIR}/unit_tests/indirect_heap/indirect_heap_fixture.cpp
  ${NEO_SOURCE_DIR}/unit_tests/indirect_heap/indirect_heap_fixture.h
  ${NEO_SOURCE_DIR}/unit_tests/ult_config_listener.cpp
  ${NEO_SOURCE_DIR}/unit_tests/ult_config_listener.h
)
add_library (igdrcl_libult_env OBJECT EXCLUDE_FROM_ALL
  ${IGDRCL_SRCS_LIB_ULT_ENV}
)

set(IGDRCL_SRCS_LIB_ULT_ENV_WINDOWS
  ${NEO_SOURCE_DIR}/unit_tests/os_interface/windows/create_wddm_memory_manager.cpp
  ${NEO_SOURCE_DIR}/unit_tests/os_interface/windows/options.cpp
  ${NEO_SOURCE_DIR}/unit_tests/os_interface/windows/sys_calls.cpp
  ${NEO_SOURCE_DIR}/unit_tests/os_interface/windows/ult_dxgi_factory.cpp
  ${NEO_SOURCE_DIR}/unit_tests/os_interface/windows/ult_dxgi_factory.h
  ${NEO_SOURCE_DIR}/unit_tests/os_interface/windows/wddm_calls.cpp
)

set(IGDRCL_SRCS_LIB_ULT_ENV_LINUX
  ${NEO_SOURCE_DIR}/unit_tests/os_interface/linux/create_drm_memory_manager.cpp
  ${NEO_SOURCE_DIR}/unit_tests/os_interface/linux/options.cpp
  ${NEO_SOURCE_DIR}/unit_tests/os_interface/linux/allocator_helper.cpp
)

if(WIN32)
  target_sources(igdrcl_libult_env PRIVATE ${IGDRCL_SRCS_LIB_ULT_ENV_WINDOWS})
  target_sources(igdrcl_libult PRIVATE ${IGDRCL_SRCS_LIB_ULT_WINDOWS})
else()
  target_sources(igdrcl_libult_env PRIVATE ${IGDRCL_SRCS_LIB_ULT_ENV_LINUX})
endif()

target_include_directories(igdrcl_libult_env PRIVATE
  $<TARGET_PROPERTY:gmock-gtest,INTERFACE_INCLUDE_DIRECTORIES>
  ${NEO_SOURCE_DIR}/unit_tests/mocks${BRANCH_DIR_SUFFIX}
)

set_property(GLOBAL PROPERTY IGDRCL_SRCS_ENABLE_TESTED_HW ${IGDRCL_SRCS_ENABLE_TESTED_HW})
set_property(GLOBAL PROPERTY IGDRCL_SRCS_ENABLE_TESTED_HW_LINUX ${IGDRCL_SRCS_ENABLE_TESTED_HW_LINUX})
set_property(GLOBAL PROPERTY IGDRCL_SRCS_LIB_ULT ${IGDRCL_SRCS_LIB_ULT})
set_property(GLOBAL PROPERTY IGDRCL_SRCS_LIB_ULT_CS ${IGDRCL_SRCS_LIB_ULT_CS})
set_property(GLOBAL PROPERTY IGDRCL_SRCS_LIB_ULT_ENV ${IGDRCL_SRCS_LIB_ULT_ENV})
set_property(GLOBAL PROPERTY IGDRCL_SRCS_LIB_ULT_ENV_LINUX ${IGDRCL_SRCS_LIB_ULT_ENV_LINUX})

foreach(target_name igdrcl_libult igdrcl_libult_cs igdrcl_libult_env)
  set_target_properties(${target_name} PROPERTIES POSITION_INDEPENDENT_CODE ON)
  set_target_properties(${target_name} PROPERTIES FOLDER ${TEST_PROJECTS_FOLDER})
  set_property(TARGET ${target_name} APPEND_STRING PROPERTY COMPILE_FLAGS ${ASAN_FLAGS} ${TSAN_FLAGS})
  target_include_directories(${target_name} PRIVATE $<TARGET_PROPERTY:${NEO_MOCKABLE_LIB_NAME},INTERFACE_INCLUDE_DIRECTORIES>)
  target_compile_definitions(${target_name} PRIVATE $<TARGET_PROPERTY:${NEO_MOCKABLE_LIB_NAME},INTERFACE_COMPILE_DEFINITIONS>)
  create_project_source_tree(${target_name})
endforeach()
