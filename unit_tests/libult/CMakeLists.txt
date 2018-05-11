# Copyright (c) 2018, Intel Corporation
#
# Permission is hereby granted, free of charge, to any person obtaining a
# copy of this software and associated documentation files (the "Software"),
# to deal in the Software without restriction, including without limitation
# the rights to use, copy, modify, merge, publish, distribute, sublicense,
# and/or sell copies of the Software, and to permit persons to whom the
# Software is furnished to do so, subject to the following conditions:
#
# The above copyright notice and this permission notice shall be included
# in all copies or substantial portions of the Software.
#
# THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS
# OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
# FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL
# THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR
# OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE,
# ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR
# OTHER DEALINGS IN THE SOFTWARE.

macro(macro_for_each_platform)
  list (APPEND IGDRCL_SRCS_ENABLE_TESTED_HW
    ${IGDRCL_SOURCE_DIR}/runtime/${GEN_TYPE_LOWER}/enable_${PLATFORM_IT_LOWER}.cpp
    ${IGDRCL_SOURCE_DIR}/runtime/${GEN_TYPE_LOWER}/enable_hw_info_config_${PLATFORM_IT_LOWER}.cpp
  )
endmacro()
macro(macro_for_each_gen)
  list(APPEND IGDRCL_SRCS_ENABLE_TESTED_HW
    ${IGDRCL_SOURCE_DIR}/unit_tests/${GEN_TYPE_LOWER}/cmd_parse_${GEN_TYPE_LOWER}.cpp
    ${IGDRCL_SOURCE_DIR}/unit_tests/${GEN_TYPE_LOWER}/gen_cmd_parse.h
    ${IGDRCL_SOURCE_DIR}/unit_tests/libult/${GEN_TYPE_LOWER}.cpp
    ${IGDRCL_SOURCE_DIR}/runtime/${GEN_TYPE_LOWER}/enable_family_full_${GEN_TYPE_LOWER}.cpp
  )
  apply_macro_for_each_platform()
endmacro()

apply_macro_for_each_gen("TESTED")

set(IGDRCL_SRCS_LIB_ULT
  ${IGDRCL_SOURCE_DIR}/unit_tests/abort.cpp
  ${IGDRCL_SOURCE_DIR}/unit_tests/helpers/built_ins_helper.cpp
  ${IGDRCL_SOURCE_DIR}/unit_tests/libult/create_tbx_sockets.cpp
  ${IGDRCL_SOURCE_DIR}/unit_tests/libult/ult_command_stream_receiver.h
  ${IGDRCL_SOURCE_DIR}/unit_tests/helpers/debug_helpers.cpp
  ${IGDRCL_SOURCE_DIR}/unit_tests/helpers/memory_management.cpp
  ${IGDRCL_SOURCE_DIR}/unit_tests/helpers/memory_management.h
  ${IGDRCL_SOURCE_DIR}/unit_tests/helpers/test_files.cpp
  ${IGDRCL_SOURCE_DIR}/unit_tests/helpers/test_files.h
  ${IGDRCL_SOURCE_DIR}/unit_tests/libult/create_tbx_sockets.cpp
  ${IGDRCL_SOURCE_DIR}/unit_tests/libult/ult_command_stream_receiver.h
  ${IGDRCL_SOURCE_DIR}/unit_tests/program/evaluate_unhandled_token_ult.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/mock_gfx_family.h
  ${CMAKE_CURRENT_SOURCE_DIR}/mock_gfx_family.cpp
)

if(SOURCE_LEVEL_DEBUGGER_HEADERS_DIR)
  list(APPEND IGDRCL_SRCS_LIB_ULT
    ${IGDRCL_SOURCE_DIR}/unit_tests/libult/source_level_debugger.cpp
    ${IGDRCL_SOURCE_DIR}/unit_tests/libult/source_level_debugger_library.cpp
    ${IGDRCL_SOURCE_DIR}/unit_tests/libult/source_level_debugger_library.h
  )
endif()

add_library (igdrcl_libult OBJECT
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
add_library (igdrcl_libult_cs OBJECT
  ${IGDRCL_SRCS_LIB_ULT_CS}
)
target_include_directories(igdrcl_libult PRIVATE
  $<TARGET_PROPERTY:gmock-gtest,INTERFACE_INCLUDE_DIRECTORIES>
)

set(IGDRCL_SRCS_LIB_ULT_ENV
  ${IGDRCL_SOURCE_DIR}/unit_tests/base_object_allocator.cpp
  ${IGDRCL_SOURCE_DIR}/unit_tests/custom_event_listener.h
  ${IGDRCL_SOURCE_DIR}/unit_tests/global_environment.cpp
  ${IGDRCL_SOURCE_DIR}/unit_tests/global_environment.h
  ${IGDRCL_SOURCE_DIR}/unit_tests/main.cpp
  ${IGDRCL_SOURCE_DIR}/unit_tests/memory_leak_listener.cpp
  ${IGDRCL_SOURCE_DIR}/unit_tests/memory_leak_listener.h
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
)
add_library (igdrcl_libult_env OBJECT
  ${IGDRCL_SRCS_LIB_ULT_ENV}
)

set(IGDRCL_SRCS_LIB_ULT_ENV_WINDOWS
  ${IGDRCL_SOURCE_DIR}/unit_tests/os_interface/windows/options.cpp
  ${IGDRCL_SOURCE_DIR}/unit_tests/os_interface/windows/sys_calls.cpp
  ${IGDRCL_SOURCE_DIR}/unit_tests/os_interface/windows/ult_dxgi_factory.cpp
  ${IGDRCL_SOURCE_DIR}/unit_tests/os_interface/windows/ult_dxgi_factory.h
  ${IGDRCL_SOURCE_DIR}/unit_tests/os_interface/windows/wddm_calls.cpp
)

set(IGDRCL_SRCS_LIB_ULT_ENV_LINUX
  ${IGDRCL_SOURCE_DIR}/unit_tests/os_interface/linux/options.cpp
  ${IGDRCL_SOURCE_DIR}/unit_tests/os_interface/linux/allocator_helper.cpp
)

if(WIN32)
  target_sources(igdrcl_libult_env PRIVATE ${IGDRCL_SRCS_LIB_ULT_ENV_WINDOWS})
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
