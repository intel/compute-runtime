#
# Copyright (C) 2017-2018 Intel Corporation
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
  )

  add_library(mock_opengl32 SHARED ${IGDRCL_SRCS_mock_opengl32})
  add_dependencies(unit_tests mock_opengl32)
  set_target_properties(mock_opengl32 PROPERTIES FOLDER "test mocks")
endif()
