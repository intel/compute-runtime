#
# Copyright (C) 2017-2018 Intel Corporation
#
# SPDX-License-Identifier: MIT
#

project(elflib_tests)

set(CLELFLIB_UNIT_TESTS_SRCS
  ${CMAKE_CURRENT_SOURCE_DIR}/elflib_tests.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/main.cpp
  ${IGDRCL_SOURCE_DIR}/unit_tests/fixtures/memory_management_fixture.cpp
  ${IGDRCL_SOURCE_DIR}/unit_tests/fixtures/memory_management_fixture.h
  ${IGDRCL_SOURCE_DIR}/unit_tests/helpers/memory_management.cpp
  ${IGDRCL_SOURCE_DIR}/unit_tests/helpers/memory_management.h
)

add_executable(elflib_tests ${CLELFLIB_UNIT_TESTS_SRCS})
target_link_libraries(elflib_tests gmock-gtest elflib)
set_property(TARGET elflib_tests APPEND_STRING PROPERTY COMPILE_FLAGS ${ASAN_FLAGS})

if(UNIX)
  target_link_libraries(elflib_tests ${IGDRCL_EXTRA_LIBS})
endif()

add_custom_target(run_elflib_tests ALL DEPENDS unit_tests elflib_tests)
add_custom_command(
  TARGET run_elflib_tests
  POST_BUILD
  COMMAND elflib_tests ${IGDRCL_TESTS_LISTENER_OPTION}
)
add_dependencies(run_unit_tests run_elflib_tests)

set_target_properties(elflib_tests PROPERTIES FOLDER "elflib")
set_target_properties(run_elflib_tests PROPERTIES FOLDER "elflib")
create_project_source_tree(elflib_tests ${IGDRCL_SOURCE_DIR}/unit_tests)
