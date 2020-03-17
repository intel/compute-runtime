#
# Copyright (C) 2020 Intel Corporation
#
# SPDX-License-Identifier: MIT
#

link_libraries(${ASAN_LIBS} ${TSAN_LIBS})

set(TARGET_NAME ${TARGET_NAME_L0}_core_tests)

append_sources_from_properties(L0_CORE_ENABLERS NEO_CORE_SRCS_LINK)

add_executable(${TARGET_NAME}
    ${NEO_SOURCE_DIR}/level_zero/core/source/dll/disallow_deferred_deleter.cpp
    ${NEO_SOURCE_DIR}/level_zero/core/source/debug_manager.cpp
    ${NEO_SOURCE_DIR}/level_zero/core/source/debugger.cpp
    ${NEO_SOURCE_DIR}/level_zero/core/source/debugger/debugger_l0.cpp
    ${NEO_SOURCE_DIR}/level_zero/core/source/compiler_interface/default_l0_cache_config.cpp
    ${NEO_SOURCE_DIR}/level_zero/core/source/compiler_interface/default_cache_config.cpp
    ${NEO_SOURCE_DIR}/shared/test/unit_test/helpers/memory_management.h
    ${NEO_SOURCE_DIR}/shared/test/unit_test/helpers/memory_management.cpp
    ${NEO_SOURCE_DIR}/shared/test/unit_test/helpers/memory_leak_listener.h
    ${NEO_SOURCE_DIR}/shared/test/unit_test/helpers/memory_leak_listener.cpp
    ${L0_CORE_ENABLERS}
)

target_sources(${TARGET_NAME} PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/CMakeLists.txt
    ${CMAKE_CURRENT_SOURCE_DIR}/main.cpp
)

target_sources(${TARGET_NAME} PRIVATE
    ${COMPUTE_RUNTIME_DIR}/opencl/test/unit_test/libult/create_command_stream.cpp
)

if (UNIX)
    target_sources(${TARGET_NAME} PRIVATE
        ${COMPUTE_RUNTIME_DIR}/opencl/test/unit_test/os_interface/linux/create_drm_memory_manager.cpp
        ${COMPUTE_RUNTIME_DIR}/opencl/test/unit_test/os_interface/linux/drm_neo_create.cpp
        ${COMPUTE_RUNTIME_DIR}/opencl/test/unit_test/os_interface/linux/options.cpp
        ${COMPUTE_RUNTIME_DIR}/opencl/test/unit_test/os_interface/linux/sys_calls_linux_ult.cpp
        ${COMPUTE_RUNTIME_DIR}/level_zero/core/source/linux/registry_path.cpp
)
else()
    target_sources(${TARGET_NAME} PRIVATE
        ${COMPUTE_RUNTIME_DIR}/opencl/test/unit_test/os_interface/windows/create_wddm_memory_manager.cpp
        ${COMPUTE_RUNTIME_DIR}/opencl/test/unit_test/os_interface/windows/options.cpp
        ${COMPUTE_RUNTIME_DIR}/opencl/test/unit_test/os_interface/windows/sys_calls.cpp
        ${COMPUTE_RUNTIME_DIR}/opencl/test/unit_test/os_interface/windows/ult_dxgi_factory.cpp
        ${COMPUTE_RUNTIME_DIR}/opencl/test/unit_test/os_interface/windows/wddm_calls.cpp
        ${COMPUTE_RUNTIME_DIR}/opencl/test/unit_test/os_interface/windows/wddm_create.cpp
        ${COMPUTE_RUNTIME_DIR}/level_zero/core/source/windows/registry_path.cpp
)
endif()

set_property(TARGET ${TARGET_NAME} APPEND_STRING PROPERTY COMPILE_FLAGS ${ASAN_FLAGS})
set_target_properties(${TARGET_NAME} PROPERTIES FOLDER ${TARGET_NAME_L0})

target_compile_definitions(${TARGET_NAME}
    PUBLIC
        DEFAULT_PRODUCT_FAMILY=${DEFAULT_PRODUCT_FAMILY}
        ZE_MAKEDLL
)

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
    target_link_libraries(${TARGET_NAME}
        pthread
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
    target_link_libraries(${TARGET_NAME}
    rt
    )
else()
    target_link_libraries(${TARGET_NAME}
    dbghelp
    )
endif()

target_link_libraries(${TARGET_NAME}
    ${NEO_MOCKABLE_LIB_NAME}
    ${NEO_SHARED_MOCKABLE_LIB_NAME}
    ${NEO_MOCKABLE_LIB_NAME}
    ${NEO_SHARED_MOCKABLE_LIB_NAME}
    compute_runtime_mockable_extra
    ${HW_LIBS_ULT}
    gmock-gtest
)

if(SKIP_NEO_UNIT_TESTS)
    add_subdirectory(${COMPUTE_RUNTIME_DIR}/opencl/test/unit_test/mock_gmm ${CMAKE_BINARY_DIR}/mock_gmm)
endif()

target_sources(${TARGET_NAME} PRIVATE $<TARGET_OBJECTS:mock_gmm>)

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

if(NOT GTEST_REPEAT)
  set(GTEST_REPEAT 1 CACHE STRING "Google test iterations")
endif()
message(STATUS "GTest repeat count set to ${GTEST_REPEAT}")

if(NOT GTEST_SHUFFLE)
  set(GTEST_SHUFFLE --gtest_shuffle --gtest_random_seed=0)
endif()
message(STATUS "GTest shuffle set to ${GTEST_SHUFFLE}")

add_subdirectoriesL0(${CMAKE_CURRENT_SOURCE_DIR} "*")

create_source_tree(${TARGET_NAME} ${L0_ROOT_DIR}/..)
