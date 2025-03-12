#
# Copyright (C) 2019-2025 Intel Corporation
#
# SPDX-License-Identifier: MIT
#

function(ADD_SUPPORTED_TEST_PRODUCT_FAMILIES_DEFINITION)
  set(NEO_SUPPORTED_PRODUCT_FAMILIES ${ALL_TESTED_PRODUCT_FAMILY})
  string(REPLACE ";" "," NEO_SUPPORTED_PRODUCT_FAMILIES "${NEO_SUPPORTED_PRODUCT_FAMILIES}")
  add_definitions(-DSUPPORTED_TEST_PRODUCT_FAMILIES=${NEO_SUPPORTED_PRODUCT_FAMILIES})
endfunction()

ADD_SUPPORTED_TEST_PRODUCT_FAMILIES_DEFINITION()
link_libraries(${ASAN_LIBS} ${TSAN_LIBS})

include(${NEO_SOURCE_DIR}/cmake/setup_ult_global_flags.cmake)

add_executable(neo_shared_tests
               ${CMAKE_CURRENT_SOURCE_DIR}/CMakeLists.txt
               ${CMAKE_CURRENT_SOURCE_DIR}/api_specific_config_ult.cpp
               ${CMAKE_CURRENT_SOURCE_DIR}/ult_specific_config.cpp
               ${NEO_SHARED_DIRECTORY}/helpers/allow_deferred_deleter.cpp
               ${NEO_SHARED_TEST_DIRECTORY}/common/common_main.cpp
               ${NEO_SHARED_TEST_DIRECTORY}/common/helpers/mock_sip_listener.cpp
               ${NEO_SHARED_TEST_DIRECTORY}/common/helpers/virtual_file_system_listener.cpp
               ${NEO_SHARED_TEST_DIRECTORY}/common/tests_configuration.h
               $<TARGET_OBJECTS:mock_aubstream>
               $<TARGET_OBJECTS:mock_gmm>
               $<TARGET_OBJECTS:neo_libult_common>
               $<TARGET_OBJECTS:neo_libult_cs>
               $<TARGET_OBJECTS:neo_libult>
               $<TARGET_OBJECTS:neo_shared_mocks>
)

add_dependencies(neo_shared_tests test_dynamic_lib)

set_property(TARGET neo_shared_tests APPEND_STRING PROPERTY COMPILE_FLAGS ${ASAN_FLAGS})
set_target_properties(neo_shared_tests PROPERTIES FOLDER "${SHARED_TEST_PROJECTS_FOLDER}")
set_property(TARGET neo_shared_tests PROPERTY ENABLE_EXPORTS TRUE)

target_include_directories(neo_shared_tests PRIVATE
                           ${NEO_SHARED_TEST_DIRECTORY}/common/test_configuration/unit_tests
                           ${ENGINE_NODE_DIR}
                           ${NEO_SHARED_TEST_DIRECTORY}/common/test_macros/header${BRANCH_DIR_SUFFIX}
                           ${NEO_SHARED_TEST_DIRECTORY}/common/helpers/includes${BRANCH_DIR_SUFFIX}
)

if(UNIX AND NOT DISABLE_WDDM_LINUX)
  target_include_directories(neo_shared_tests PUBLIC ${WDK_INCLUDE_PATHS})
endif()

if(WIN32)
  target_link_libraries(neo_shared_tests dbghelp)
endif()

target_link_libraries(neo_shared_tests
                      gmock-gtest
                      ${NEO_SHARED_MOCKABLE_LIB_NAME}
                      ${NEO_EXTRA_LIBS}
)

if(MSVC)
  set_target_properties(neo_shared_tests PROPERTIES
                        VS_DEBUGGER_COMMAND_ARGUMENTS "--gtest_filter=* --gtest_catch_exceptions=0 --enable_default_listener --disable_pagefaulting_tests"
                        VS_DEBUGGER_WORKING_DIRECTORY "$(OutDir)"
  )
endif()

if(COMPILER_SUPPORTS_SSE42)
  set_source_files_properties(helpers/uint16_sse4_tests.cpp PROPERTIES COMPILE_FLAGS -msse4.2)
endif()

add_subdirectory_unique(mocks)
add_subdirectories()

target_sources(neo_shared_tests PRIVATE
               $<TARGET_OBJECTS:neo_unit_tests_config>
               $<TARGET_OBJECTS:${BUILTINS_BINARIES_STATELESS_LIB_NAME}>
               $<TARGET_OBJECTS:${BUILTINS_BINARIES_HEAPLESS_LIB_NAME}>
               $<TARGET_OBJECTS:${BUILTINS_BINARIES_BINDFUL_LIB_NAME}>
               $<TARGET_OBJECTS:${BUILTINS_BINARIES_BINDLESS_LIB_NAME}>
)
add_dependencies(unit_tests prepare_test_kernels_for_shared)
add_dependencies(unit_tests neo_shared_tests)

create_project_source_tree(neo_shared_tests)
