# Copyright (c) 2017 - 2018, Intel Corporation
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
