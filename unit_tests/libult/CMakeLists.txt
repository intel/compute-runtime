#
# Copyright (C) 2018-2019 Intel Corporation
#
# SPDX-License-Identifier: MIT
#

macro(macro_for_each_gen)
  list(APPEND IGDRCL_SRCS_ENABLE_TESTED_HW
    ${IGDRCL_SOURCE_DIR}/unit_tests/${GEN_TYPE_LOWER}/cmd_parse_${GEN_TYPE_LOWER}.cpp
    ${IGDRCL_SOURCE_DIR}/unit_tests/libult/${GEN_TYPE_LOWER}.cpp
    ${IGDRCL_SOURCE_DIR}/runtime/${GEN_TYPE_LOWER}/enable_family_full_${GEN_TYPE_LOWER}.cpp
    ${IGDRCL_SOURCE_DIR}/runtime/${GEN_TYPE_LOWER}/enable_hw_info_config_${GEN_TYPE_LOWER}.cpp
    ${IGDRCL_SOURCE_DIR}/runtime/${GEN_TYPE_LOWER}/enable_${GEN_TYPE_LOWER}.cpp
  )
endmacro()

apply_macro_for_each_gen("TESTED")

set(IGDRCL_SRCS_LIB_ULT
  ${IGDRCL_SOURCE_DIR}/unit_tests/abort.cpp
  ${IGDRCL_SOURCE_DIR}/unit_tests/helpers/built_ins_helper.cpp
  ${IGDRCL_SOURCE_DIR}/unit_tests/helpers/execution_environment_helper.cpp
  ${IGDRCL_SOURCE_DIR}/unit_tests/helpers/execution_environment_helper.h
  ${IGDRCL_SOURCE_DIR}/unit_tests/libult/create_tbx_sockets.cpp
  ${IGDRCL_SOURCE_DIR}/unit_tests/libult/ult_command_stream_receiver.h
  ${IGDRCL_SOURCE_DIR}/unit_tests/helpers/debug_helpers.cpp
  ${IGDRCL_SOURCE_DIR}/unit_tests/helpers/test_files.cpp
  ${IGDRCL_SOURCE_DIR}/unit_tests/helpers/test_files.h
  ${IGDRCL_SOURCE_DIR}/unit_tests/libult/create_tbx_sockets.cpp
  ${IGDRCL_SOURCE_DIR}/unit_tests/libult/ult_command_stream_receiver.h
  ${IGDRCL_SOURCE_DIR}/unit_tests/libult/source_level_debugger.cpp
  ${IGDRCL_SOURCE_DIR}/unit_tests/libult/source_level_debugger_library.cpp
  ${IGDRCL_SOURCE_DIR}/unit_tests/libult/source_level_debugger_library.h
  ${IGDRCL_SOURCE_DIR}/unit_tests/program/evaluate_unhandled_token_ult.cpp
  ${IGDRCL_SOURCE_DIR}/unit_tests/utilities/debug_settings_reader_creator.cpp
  ${NEO_CORE_DIRECTORY}/unit_tests/helpers/memory_management.h
  ${NEO_CORE_DIRECTORY}/unit_tests/helpers/memory_management.cpp
  ${NEO_CORE_DIRECTORY}/unit_tests/helpers/memory_leak_listener.cpp
  ${NEO_CORE_DIRECTORY}/unit_tests/helpers/memory_leak_listener.h
)

set(IGDRCL_SRCS_LIB_ULT_WINDOWS
  ${IGDRCL_SOURCE_DIR}/unit_tests/os_interface/windows/mock_environment_variables.cpp
)

add_library (igdrcl_libult OBJECT EXCLUDE_FROM_ALL
  ${IGDRCL_SRCS_LIB_ULT}
  ${IGDRCL_SRCS_ENABLE_TESTED_HW}
)
if(UNIX)
  target_sources(igdrcl_libult PRIVATE ${IGDRCL_SRCS_ENABLE_TESTED_HW_LINUX})
endif()

set(IGDRCL_SRCS_LIB_ULT_CS
  ${IGDRCL_SOURCE_DIR}/unit_tests/libult/create_command_stream.cpp
  ${IGDRCL_SOURCE_DIR}/unit_tests/libult/create_command_stream.h
)
add_library (igdrcl_libult_cs OBJECT EXCLUDE_FROM_ALL
  ${IGDRCL_SRCS_LIB_ULT_CS}
)
target_include_directories(igdrcl_libult PRIVATE
  $<TARGET_PROPERTY:gmock-gtest,INTERFACE_INCLUDE_DIRECTORIES>
)

set(IGDRCL_SRCS_LIB_ULT_ENV
  ${IGDRCL_SOURCE_DIR}/unit_tests/custom_event_listener.h
  ${IGDRCL_SOURCE_DIR}/unit_tests/global_environment.cpp
  ${IGDRCL_SOURCE_DIR}/unit_tests/global_environment.h
  ${IGDRCL_SOURCE_DIR}/unit_tests/main.cpp
  ${IGDRCL_SOURCE_DIR}/unit_tests/options.cpp
  ${IGDRCL_SOURCE_DIR}/unit_tests/command_queue/command_queue_fixture.cpp
  ${IGDRCL_SOURCE_DIR}/unit_tests/command_queue/command_queue_fixture.h
  ${IGDRCL_SOURCE_DIR}/unit_tests/fixtures/memory_management_fixture.cpp
  ${IGDRCL_SOURCE_DIR}/unit_tests/fixtures/memory_management_fixture.h
  ${IGDRCL_SOURCE_DIR}/unit_tests/fixtures/built_in_fixture.cpp
  ${IGDRCL_SOURCE_DIR}/unit_tests/fixtures/built_in_fixture.h
  ${IGDRCL_SOURCE_DIR}/unit_tests/fixtures/buffer_fixture.cpp
  ${IGDRCL_SOURCE_DIR}/unit_tests/fixtures/buffer_fixture.h
  ${IGDRCL_SOURCE_DIR}/unit_tests/fixtures/device_fixture.cpp
  ${IGDRCL_SOURCE_DIR}/unit_tests/fixtures/device_fixture.h
  ${IGDRCL_SOURCE_DIR}/unit_tests/fixtures/program_fixture.cpp
  ${IGDRCL_SOURCE_DIR}/unit_tests/fixtures/program_fixture.h
  ${IGDRCL_SOURCE_DIR}/unit_tests/helpers/kernel_binary_helper.cpp
  ${IGDRCL_SOURCE_DIR}/unit_tests/helpers/kernel_binary_helper.h
  ${IGDRCL_SOURCE_DIR}/unit_tests/indirect_heap/indirect_heap_fixture.cpp
  ${IGDRCL_SOURCE_DIR}/unit_tests/indirect_heap/indirect_heap_fixture.h
  ${IGDRCL_SOURCE_DIR}/unit_tests/ult_config_listener.cpp
  ${IGDRCL_SOURCE_DIR}/unit_tests/ult_config_listener.h
)
add_library (igdrcl_libult_env OBJECT EXCLUDE_FROM_ALL
  ${IGDRCL_SRCS_LIB_ULT_ENV}
)

set(IGDRCL_SRCS_LIB_ULT_ENV_WINDOWS
  ${IGDRCL_SOURCE_DIR}/unit_tests/os_interface/windows/create_wddm_memory_manager.cpp
  ${IGDRCL_SOURCE_DIR}/unit_tests/os_interface/windows/options.cpp
  ${IGDRCL_SOURCE_DIR}/unit_tests/os_interface/windows/sys_calls.cpp
  ${IGDRCL_SOURCE_DIR}/unit_tests/os_interface/windows/ult_dxgi_factory.cpp
  ${IGDRCL_SOURCE_DIR}/unit_tests/os_interface/windows/ult_dxgi_factory.h
  ${IGDRCL_SOURCE_DIR}/unit_tests/os_interface/windows/wddm_calls.cpp
)

set(IGDRCL_SRCS_LIB_ULT_ENV_LINUX
  ${IGDRCL_SOURCE_DIR}/unit_tests/os_interface/linux/create_drm_memory_manager.cpp
  ${IGDRCL_SOURCE_DIR}/unit_tests/os_interface/linux/options.cpp
  ${IGDRCL_SOURCE_DIR}/unit_tests/os_interface/linux/allocator_helper.cpp
)

if(WIN32)
  target_sources(igdrcl_libult_env PRIVATE ${IGDRCL_SRCS_LIB_ULT_ENV_WINDOWS})
  target_sources(igdrcl_libult PRIVATE ${IGDRCL_SRCS_LIB_ULT_WINDOWS})
else()
  target_sources(igdrcl_libult_env PRIVATE ${IGDRCL_SRCS_LIB_ULT_ENV_LINUX})
endif()

target_include_directories(igdrcl_libult_env PRIVATE
  $<TARGET_PROPERTY:gmock-gtest,INTERFACE_INCLUDE_DIRECTORIES>
  ${IGDRCL_SOURCE_DIR}/unit_tests/mocks${BRANCH_DIR_SUFFIX}
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
  create_project_source_tree(${target_name} ${IGDRCL_SOURCE_DIR}/runtime ${IGDRCL_SOURCE_DIR}/unit_tests)
endforeach()
