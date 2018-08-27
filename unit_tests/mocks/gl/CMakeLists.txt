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
