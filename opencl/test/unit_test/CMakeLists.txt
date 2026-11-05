#
# Copyright (C) 2018-2026 Intel Corporation
#
# SPDX-License-Identifier: MIT
#

project(igdrcl_tests)

set(OPENCL_TEST_PROJECTS_FOLDER "opencl runtime")
set(PLATFORM_SPECIFIC_TEST_TARGETS_FOLDER "${OPENCL_TEST_PROJECTS_FOLDER}/test platforms")
set(OPENCL_UNIT_TEST_DIR ${CMAKE_CURRENT_SOURCE_DIR})

include(${NEO_SOURCE_DIR}/cmake/setup_ult_global_flags.cmake)

function(ADD_SUPPORTED_TEST_PRODUCT_FAMILIES_DEFINITION)
  set(NEO_SUPPORTED_TEST_PRODUCT_FAMILIES ${ALL_TESTED_PRODUCT_FAMILY})
  string(REPLACE ";" "," NEO_SUPPORTED_TEST_PRODUCT_FAMILIES "${NEO_SUPPORTED_TEST_PRODUCT_FAMILIES}")
  add_definitions(-DSUPPORTED_TEST_PRODUCT_FAMILIES=${NEO_SUPPORTED_TEST_PRODUCT_FAMILIES})
endfunction()

ADD_SUPPORTED_TEST_PRODUCT_FAMILIES_DEFINITION()
link_libraries(${ASAN_LIBS} ${TSAN_LIBS})

add_custom_target(run_unit_tests ALL)

add_custom_target(run_ocl_tests)
set_target_properties(run_ocl_tests PROPERTIES FOLDER ${PLATFORM_SPECIFIC_TEST_TARGETS_FOLDER})

set(IGDRCL_SRCS_tests_local
    ${CMAKE_CURRENT_SOURCE_DIR}/CMakeLists.txt
    ${NEO_SHARED_TEST_DIRECTORY}/common/tests_configuration.h
)

add_subdirectory(libult)

hide_subdir(libult)
hide_subdir(linux)

if(UNIX)
  add_subdirectory(linux)
  add_custom_command(
                     TARGET run_ocl_tests
                     POST_BUILD
                     COMMAND echo running tests for linux dynamic library - .so in ${TargetDir}
                     COMMAND ${NEO_RUN_INTERCEPTOR_LIST} igdrcl_linux_dll_tests ${NEO_TESTS_LISTENER_OPTION}
                     COMMAND WORKING_DIRECTORY ${TargetDir}
  )
endif()

set(NEO_IGDRCL_TESTS__TARGET_OBJECTS
    $<TARGET_OBJECTS:neo_libult_common>
    $<TARGET_OBJECTS:igdrcl_libult>
    $<TARGET_OBJECTS:neo_libult_cs>
    $<TARGET_OBJECTS:neo_libult>
    $<TARGET_OBJECTS:igdrcl_libult_env>
    $<TARGET_OBJECTS:mock_aubstream>
    $<TARGET_OBJECTS:mock_gmm>
    $<TARGET_OBJECTS:${SHARINGS_ENABLE_LIB_NAME}>
    $<TARGET_OBJECTS:${BUILTINS_SOURCES_LIB_NAME}>
    $<TARGET_OBJECTS:${BUILTINS_BINARIES_LIB_NAME}>
    $<TARGET_OBJECTS:neo_shared_mocks>
    $<TARGET_OBJECTS:neo_unit_tests_config>
)

add_executable(igdrcl_tests
               ${NEO_IGDRCL_TESTS__TARGET_OBJECTS}
               ${IGDRCL_SRCS_tests_local}
)

hide_subdir(gen_common)
add_subdirectory(gen_common)

if(NOT GTEST_EXCEPTION_OPTIONS)
  set(GTEST_EXCEPTION_OPTIONS --gtest_catch_exceptions=1)
endif()
message(STATUS "GTest exception options set to ${GTEST_EXCEPTION_OPTIONS}")

if(GTEST_FILTERING_PATTERN)
  set(GTEST_FILTER_OPTION "--gtest_filter=${GTEST_FILTERING_PATTERN}")
  message(STATUS "GTest filter for regular tests: ${GTEST_FILTERING_PATTERN}")
endif()

target_link_libraries(igdrcl_tests ${NEO_MOCKABLE_LIB_NAME} ${NEO_SHARED_MOCKABLE_LIB_NAME})
target_link_libraries(igdrcl_tests igdrcl_mocks)

target_include_directories(igdrcl_tests PRIVATE
                           ${NEO_SOURCE_DIR}/opencl/test/unit_test/mocks${BRANCH_DIR_SUFFIX}
                           ${ENGINE_NODE_DIR}
                           ${KHRONOS_GL_HEADERS_DIR}
                           ${NEO_SHARED_TEST_DIRECTORY}/common/test_configuration/unit_tests
)
target_link_libraries(igdrcl_tests neo_gtest ${NEO_EXTRA_LIBS})

add_dependencies(unit_tests
                 igdrcl_tests
)

set_target_properties(igdrcl_tests PROPERTIES FOLDER ${OPENCL_TEST_PROJECTS_FOLDER})
set_property(TARGET igdrcl_tests APPEND_STRING PROPERTY COMPILE_FLAGS ${ASAN_FLAGS})
if(UNIX)
  set_property(TARGET igdrcl_tests APPEND_STRING PROPERTY COMPILE_FLAGS " -g")
endif()
set_property(TARGET igdrcl_tests PROPERTY ENABLE_EXPORTS TRUE)

set_target_properties(unit_tests PROPERTIES FOLDER ${OPENCL_TEST_PROJECTS_FOLDER})
set_target_properties(run_unit_tests PROPERTIES FOLDER ${OPENCL_TEST_PROJECTS_FOLDER})

target_include_directories(igdrcl_tests BEFORE PRIVATE
                           ${NEO_SHARED_TEST_DIRECTORY}/common/test_macros/header${BRANCH_DIR_SUFFIX}
                           ${NEO_SHARED_TEST_DIRECTORY}/common/helpers/includes${BRANCH_DIR_SUFFIX}
)

add_subdirectories()
create_project_source_tree(igdrcl_tests)

set(UltPchHeader "${CMAKE_CURRENT_SOURCE_DIR}/igdrcl_tests_pch.h")
set(UltPchSource "${CMAKE_CURRENT_SOURCE_DIR}/igdrcl_tests_pch.cpp")
get_target_property(UltSources igdrcl_tests SOURCES)

if(MSVC AND NOT DISABLE_ULT_PCH_WIN)
  set(UltPchBinary "${CMAKE_CURRENT_BINARY_DIR}/igdrcl_tests_pch.pch")

  set(IGDRCL_SRCS_ult_pch ${UltPchSource} ${UltPchHeader})
  target_sources(igdrcl_tests PRIVATE ${IGDRCL_SRCS_ult_pch})

  set_source_files_properties(${UltSources}
                              PROPERTIES COMPILE_FLAGS "/Yu${UltPchHeader} /FI${UltPchHeader} /Fp${UltPchBinary}"
                              OBJECT_DEPENDS "${UltPchBinary}"
  )

  set_source_files_properties(${UltPchSource}
                              PROPERTIES COMPILE_FLAGS "/Yc${UltPchHeader} /FI${UltPchHeader} /Fp${UltPchBinary}"
                              OBJECT_OUTPUTS "${UltPchBinary}"
  )
elseif(USE_ULT_PCH)
  set(UltPchHeaderInBuildDir "${CMAKE_CURRENT_BINARY_DIR}/igdrcl_tests_pch.h")
  set(UltPchBinaryGch "${UltPchHeaderInBuildDir}.gch")
  set(UltPchBinary "${CMAKE_CURRENT_BINARY_DIR}/CMakeFiles/igdrcl_ult_pch.dir/igdrcl_tests_pch.h.o")

  add_library(igdrcl_ult_pch STATIC EXCLUDE_FROM_ALL ${UltPchHeader})
  add_dependencies(igdrcl_tests igdrcl_ult_pch)

  target_include_directories(igdrcl_ult_pch PRIVATE
                             $<TARGET_PROPERTY:${NEO_MOCKABLE_LIB_NAME},INTERFACE_INCLUDE_DIRECTORIES>
                             $<TARGET_PROPERTY:igdrcl_tests,INCLUDE_DIRECTORIES>
  )
  target_compile_definitions(igdrcl_ult_pch PRIVATE $<TARGET_PROPERTY:${NEO_MOCKABLE_LIB_NAME},INTERFACE_COMPILE_DEFINITIONS>)

  target_include_directories(igdrcl_tests PRIVATE ${CMAKE_CURRENT_BINARY_DIR})

  if(NOT USE_ASAN)
    set_source_files_properties(${UltSources}
                                PROPERTIES COMPILE_FLAGS "-include ${UltPchHeaderInBuildDir} -msse4"
                                OBJECT_DEPENDS ${UltPchBinaryGch}
    )
  endif()

  set_source_files_properties(${UltPchHeader}
                              PROPERTIES LANGUAGE "CXX"
                              COMPILE_FLAGS "-x c++-header -msse4 -gdwarf-2"
  )

  add_custom_command(
                     OUTPUT ${UltPchBinaryGch}
                     COMMAND cp "${UltPchHeader}" "${UltPchHeaderInBuildDir}"
                     COMMAND cp "${UltPchBinary}" "${UltPchBinaryGch}"
                     DEPENDS ${UltPchBinary}
  )
endif()
# !!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!
# !!                                                                             !!
# !!              DONT ADD ANY SOURCES HERE!                                     !!
# !!                                                                             !!
# !!      You are below PCH logic!                                               !!
# !!      This is to keep PCH dependencies correctly without creating new target !!
# !!                                                                             !!
# !!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!
