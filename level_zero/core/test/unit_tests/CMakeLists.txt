#
# Copyright (C) 2020 Intel Corporation
#
# SPDX-License-Identifier: MIT
#

link_libraries(${ASAN_LIBS} ${TSAN_LIBS})

set(TARGET_NAME ${TARGET_NAME_L0}_core_tests)

append_sources_from_properties(L0_CORE_ENABLERS NEO_CORE_SRCS_LINK)

function(ADD_SUPPORTED_TEST_PRODUCT_FAMILIES_DEFINITION)
    set(L0_TESTED_PRODUCT_FAMILIES ${ALL_TESTED_PRODUCT_FAMILY})
    string(REPLACE ";" "," L0_TESTED_PRODUCT_FAMILIES "${L0_TESTED_PRODUCT_FAMILIES}")
    add_definitions(-DSUPPORTED_TEST_PRODUCT_FAMILIES=${L0_TESTED_PRODUCT_FAMILIES})
endfunction()

ADD_SUPPORTED_TEST_PRODUCT_FAMILIES_DEFINITION()

add_executable(${TARGET_NAME}
    ${NEO_SOURCE_DIR}/level_zero/core/source/dll/disallow_deferred_deleter.cpp
    ${NEO_SOURCE_DIR}/shared/test/unit_test/helpers/memory_management.h
    ${NEO_SOURCE_DIR}/shared/test/unit_test/helpers/memory_management.cpp
    ${NEO_SOURCE_DIR}/shared/test/unit_test/helpers/memory_leak_listener.h
    ${NEO_SOURCE_DIR}/shared/test/unit_test/helpers/memory_leak_listener.cpp
    ${L0_CORE_ENABLERS}
)

target_sources(${TARGET_NAME} PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/CMakeLists.txt
    ${CMAKE_CURRENT_SOURCE_DIR}/main.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/mock.h
    ${CMAKE_CURRENT_SOURCE_DIR}/test_mode.h
    ${CMAKE_CURRENT_SOURCE_DIR}/ult_configuration.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/white_box.h
    ${NEO_SHARED_TEST_DIRECTORY}/unit_test/tests_configuration.h
)

target_sources(${TARGET_NAME} PRIVATE
    ${COMPUTE_RUNTIME_DIR}/opencl/test/unit_test/libult/create_command_stream.cpp
    $<TARGET_OBJECTS:${L0_MOCKABLE_LIB_NAME}>
)

if (UNIX)
    target_sources(${TARGET_NAME} PRIVATE
        ${COMPUTE_RUNTIME_DIR}/opencl/test/unit_test/os_interface/linux/create_drm_memory_manager.cpp
        ${COMPUTE_RUNTIME_DIR}/opencl/test/unit_test/os_interface/linux/drm_neo_create.cpp
        ${COMPUTE_RUNTIME_DIR}/opencl/test/unit_test/os_interface/linux/options.cpp
        ${COMPUTE_RUNTIME_DIR}/opencl/test/unit_test/os_interface/linux/sys_calls_linux_ult.cpp
    )
else()
    target_sources(${TARGET_NAME} PRIVATE
        ${COMPUTE_RUNTIME_DIR}/opencl/test/unit_test/os_interface/windows/create_wddm_memory_manager.cpp
        ${COMPUTE_RUNTIME_DIR}/opencl/test/unit_test/os_interface/windows/options.cpp
        ${COMPUTE_RUNTIME_DIR}/opencl/test/unit_test/os_interface/windows/sys_calls.cpp
        ${COMPUTE_RUNTIME_DIR}/opencl/test/unit_test/os_interface/windows/ult_dxgi_factory.cpp
        ${COMPUTE_RUNTIME_DIR}/opencl/test/unit_test/os_interface/windows/wddm_calls.cpp
        ${COMPUTE_RUNTIME_DIR}/opencl/test/unit_test/os_interface/windows/wddm_create.cpp
    )
endif()

set_property(TARGET ${TARGET_NAME} APPEND_STRING PROPERTY COMPILE_FLAGS ${ASAN_FLAGS})
set_target_properties(${TARGET_NAME} PROPERTIES FOLDER ${TARGET_NAME_L0})

add_subdirectoriesL0(${CMAKE_CURRENT_SOURCE_DIR} "*")

target_compile_definitions(${TARGET_NAME} PRIVATE $<TARGET_PROPERTY:${L0_MOCKABLE_LIB_NAME},INTERFACE_COMPILE_DEFINITIONS>)

target_include_directories(${TARGET_NAME}
    BEFORE
    PRIVATE
        $<TARGET_PROPERTY:${TARGET_NAME_L0},SOURCE_DIR>/core
        $<TARGET_PROPERTY:${TARGET_NAME_L0},SOURCE_DIR>/tools
        $<TARGET_PROPERTY:${TARGET_NAME_L0},SOURCE_DIR>/../../../../instrumentation/inc/common/instrumentation/api/
        $<TARGET_PROPERTY:${TARGET_NAME_L0},SOURCE_DIR>/core/os_interface
        ${NEO_SHARED_TEST_DIRECTORY}/unit_test/test_macros${BRANCH_DIR_SUFFIX}
)

if (UNIX)
    target_include_directories(${TARGET_NAME}
        BEFORE
        PRIVATE
            $<TARGET_PROPERTY:${TARGET_NAME_L0},SOURCE_DIR>/core/os_interface/linux
            $<TARGET_PROPERTY:${TARGET_NAME_L0},SOURCE_DIR>/tools/linux
            ${COMPUTE_RUNTIME_DIR}/opencl/test/unit_test/linux
    )
else()
    target_include_directories(${TARGET_NAME}
        BEFORE
        PRIVATE
            $<TARGET_PROPERTY:${TARGET_NAME_L0},SOURCE_DIR>/core/os_interface/windows
            $<TARGET_PROPERTY:${TARGET_NAME_L0},SOURCE_DIR>/tools/windows
            ${COMPUTE_RUNTIME_DIR}/opencl/test/unit_test/windows
    )
endif()

if (UNIX)
    target_link_libraries(${TARGET_NAME} pthread rt)
else()
    target_link_libraries(${TARGET_NAME} dbghelp)
endif()

target_link_libraries(${TARGET_NAME}
    ${NEO_STATICALLY_LINKED_LIBRARIES_MOCKABLE}
    compute_runtime_mockable_extra
    ${HW_LIBS_ULT}
    gmock-gtest
)

if(SKIP_NEO_UNIT_TESTS)
    add_subdirectory(${COMPUTE_RUNTIME_DIR}/opencl/test/unit_test/mock_gmm ${CMAKE_BINARY_DIR}/mock_gmm)
endif()

target_sources(${TARGET_NAME} PRIVATE
    $<TARGET_OBJECTS:mock_gmm>
    $<TARGET_OBJECTS:${TARGET_NAME_L0}_mocks>
)

option(L0_ULT_VERBOSE "Use the default/verbose test output" OFF)
if(NOT L0_ULT_VERBOSE)
  set(L0_TESTS_LISTENER_OPTION "--disable_default_listener")
else()
  set(L0_TESTS_LISTENER_OPTION "--enable_default_listener")
endif()

if(L0_ULT_FILTER)
  set(L0_TESTS_FILTER_OPTION "--gtest_filter=*${L0_ULT_FILTER}*")
else()
  set(L0_TESTS_FILTER_OPTION "--gtest_filter=*")
endif()

if(MSVC)
    set_target_properties(${TARGET_NAME}
        PROPERTIES
            VS_DEBUGGER_COMMAND_ARGUMENTS "${L0_TESTS_FILTER_OPTION} --gtest_catch_exceptions=0 ${L0_TESTS_LISTENER_OPTION}"
            VS_DEBUGGER_WORKING_DIRECTORY "$(OutDir)"
    )
    set(RUN_ULT_CMD $<TARGET_FILE:${TARGET_NAME}>)
else()
    set(RUN_ULT_CMD LD_LIBRARY_PATH=$<TARGET_FILE_DIR:${TARGET_NAME}> $<TARGET_FILE:${TARGET_NAME}>)
endif()

create_source_tree(${TARGET_NAME} ${L0_ROOT_DIR}/..)
