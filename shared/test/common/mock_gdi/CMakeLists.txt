#
# Copyright (C) 2018-2021 Intel Corporation
#
# SPDX-License-Identifier: MIT
#

if(WIN32)
  project(mock_gdi)

  set(DEF_FILE "${CMAKE_CURRENT_SOURCE_DIR}/gdi32_mock.def")

  # Setting up our local list of test files
  set(NEO_SHARED_TESTS_mock_gdi
      ${CMAKE_CURRENT_SOURCE_DIR}/CMakeLists.txt
      ${CMAKE_CURRENT_SOURCE_DIR}/mock_gdi.cpp
      ${CMAKE_CURRENT_SOURCE_DIR}/mock_gdi.h
      ${CMAKE_CURRENT_SOURCE_DIR}/mock_gdi_gfx_partition.cpp
      ${DEF_FILE}
  )

  # this is a DLL
  add_library(mock_gdi SHARED ${NEO_SHARED_TESTS_mock_gdi})

  add_definitions(-DINSIDE_PLUGIN)

  target_include_directories(mock_gdi PRIVATE
                             ${WDK_INCLUDE_PATHS}
                             ${NEO__GMM_INCLUDE_DIR}
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
endif()
