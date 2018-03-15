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

if(WIN32)
  project(mock_gdi)

  set(DEF_FILE "${CMAKE_CURRENT_SOURCE_DIR}/gdi32_mock.def")

  # Setting up our local list of test files
  set(IGDRCL_SRCS_tests_mock_gdi
    ${CMAKE_CURRENT_SOURCE_DIR}/CMakeLists.txt
    ${CMAKE_CURRENT_SOURCE_DIR}/mock_gdi.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/mock_gdi.h
    ${DEF_FILE}
  )

  # this is a DLL
  add_library(mock_gdi SHARED ${IGDRCL_SRCS_tests_mock_gdi})

  add_definitions(-DINSIDE_PLUGIN)

  target_include_directories(mock_gdi PRIVATE
    ${WDK_INCLUDE_PATHS}
    ${UMKM_SHAREDDATA_INCLUDE_PATHS}
  )

  set_target_properties(
    mock_gdi
    PROPERTIES DEBUG_OUTPUT_NAME "gdi32_mock"
    RELEASE_OUTPUT_NAME "gdi32_mock"
    RELEASEINTERNAL_OUTPUT_NAME "gdi32_mock"
    OUTPUT_NAME "gdi32_mock"
  )

  create_project_source_tree(mock_gdi)

  set_target_properties(mock_gdi PROPERTIES FOLDER "test mocks")

  target_compile_definitions(mock_gdi PUBLIC MOCKABLE_VIRTUAL=virtual)

  add_dependencies(unit_tests mock_gdi)
endif()
