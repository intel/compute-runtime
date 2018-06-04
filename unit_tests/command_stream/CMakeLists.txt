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

set(IGDRCL_SRCS_tests_command_stream
  ${CMAKE_CURRENT_SOURCE_DIR}/CMakeLists.txt
  ${CMAKE_CURRENT_SOURCE_DIR}/aub_command_stream_receiver_tests.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/cmd_parse_tests.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/command_stream_fixture.h
  ${CMAKE_CURRENT_SOURCE_DIR}/command_stream_receiver_hw_tests.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/command_stream_receiver_hw_tests.inl
  ${CMAKE_CURRENT_SOURCE_DIR}/command_stream_receiver_tests.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/command_stream_receiver_flush_task_tests.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/command_stream_receiver_flush_task_gmock_tests.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/command_stream_receiver_with_aub_dump_tests.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/create_command_stream_receiver_tests.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/get_devices_tests.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/linear_stream_fixture.h
  ${CMAKE_CURRENT_SOURCE_DIR}/linear_stream_tests.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/submissions_aggregator_tests.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/tbx_command_stream_fixture.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/tbx_command_stream_fixture.h
  ${CMAKE_CURRENT_SOURCE_DIR}/tbx_command_stream_tests.cpp
)
target_sources(igdrcl_tests PRIVATE ${IGDRCL_SRCS_tests_command_stream})
