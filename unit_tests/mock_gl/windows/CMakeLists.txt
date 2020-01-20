#
# Copyright (C) 2017-2020 Intel Corporation
#
# SPDX-License-Identifier: MIT
#

if(WIN32)
  set(IGDRCL_SRCS_mock_opengl32
    ${CMAKE_CURRENT_SOURCE_DIR}/mock_opengl32.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/mock_opengl32.def
    ${NEO_SOURCE_DIR}/unit_tests/helpers/windows/mock_function.cpp
  )

  add_library(mock_opengl32 SHARED ${IGDRCL_SRCS_mock_opengl32})
  add_dependencies(unit_tests mock_opengl32)
  add_dependencies(igdrcl_tests mock_opengl32)
  set_target_properties(mock_opengl32 PROPERTIES FOLDER "test mocks")
  target_include_directories(mock_opengl32 PRIVATE $<TARGET_PROPERTY:${NEO_MOCKABLE_LIB_NAME},INTERFACE_INCLUDE_DIRECTORIES>)
  target_compile_definitions(mock_opengl32 PRIVATE $<TARGET_PROPERTY:${NEO_MOCKABLE_LIB_NAME},INTERFACE_COMPILE_DEFINITIONS>)
endif()

