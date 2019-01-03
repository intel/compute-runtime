#
# Copyright (C) 2017-2019 Intel Corporation
#
# SPDX-License-Identifier: MIT
#

if(WIN32)
  list (APPEND IGDRCL_SRCS_tests_mocks
    ${CMAKE_CURRENT_SOURCE_DIR}/mock_gl_sharing.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/mock_gl_sharing.h
    ${CMAKE_CURRENT_SOURCE_DIR}/mock_gl_arb_sync_event.h
  )

  set(IGDRCL_SRCS_tests_mocks "${IGDRCL_SRCS_tests_mocks}" PARENT_SCOPE)

  set(IGDRCL_SRCS_mock_opengl32
    ${CMAKE_CURRENT_SOURCE_DIR}/mock_opengl32.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/mock_opengl32.def
    ${IGDRCL_SOURCE_DIR}/unit_tests/helpers/windows/mock_function.cpp
  )

  add_library(mock_opengl32 SHARED ${IGDRCL_SRCS_mock_opengl32})
  add_dependencies(unit_tests mock_opengl32)
  set_target_properties(mock_opengl32 PROPERTIES FOLDER "test mocks")
  target_include_directories(mock_opengl32 PRIVATE $<TARGET_PROPERTY:${NEO_MOCKABLE_LIB_NAME},INTERFACE_INCLUDE_DIRECTORIES>) 
  target_compile_definitions(mock_opengl32 PRIVATE $<TARGET_PROPERTY:${NEO_MOCKABLE_LIB_NAME},INTERFACE_COMPILE_DEFINITIONS>) 
endif()

