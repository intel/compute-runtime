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

if(WIN32)
project(igdrcl_windows_dll_tests)

add_executable(igdrcl_windows_dll_tests
  ${IGDRCL_SOURCE_DIR}/runtime/os_interface/windows/wddm/wddm_create.cpp
  ${IGDRCL_SOURCE_DIR}/unit_tests/ult_configuration.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/wddm_create_tests.cpp
  $<TARGET_OBJECTS:igdrcl_libult>
  $<TARGET_OBJECTS:igdrcl_libult_cs>
  $<TARGET_OBJECTS:igdrcl_libult_env>
)

target_link_libraries(igdrcl_windows_dll_tests ${NEO_MOCKABLE_LIB_NAME} igdrcl_mocks gmock-gtest ${IGDRCL_EXTRA_LIBS})

target_include_directories(igdrcl_windows_dll_tests PRIVATE
  ${IGDRCL_SOURCE_DIR}/unit_tests/gen_common
)

create_project_source_tree(igdrcl_windows_dll_tests ${IGDRCL_SOURCE_DIR}/runtime ${IGDRCL_SOURCE_DIR}/unit_tests)

add_custom_target(run_windows_dll_tests ALL DEPENDS unit_tests igdrcl_windows_dll_tests)
add_custom_command(
  TARGET run_windows_dll_tests
  POST_BUILD
  COMMAND WORKING_DIRECTORY ${TargetDir}
  COMMAND echo Target Directory is: ${TargetDir}
  COMMAND echo Running Windows dll tests
  COMMAND igdrcl_windows_dll_tests
)
add_dependencies(run_unit_tests run_windows_dll_tests)

set_target_properties(igdrcl_windows_dll_tests PROPERTIES FOLDER ${TEST_PROJECTS_FOLDER})
set_target_properties(run_windows_dll_tests PROPERTIES FOLDER ${TEST_PROJECTS_FOLDER})
endif()