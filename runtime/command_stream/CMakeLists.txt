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

set(RUNTIME_SRCS_COMMAND_STREAM
  ${CMAKE_CURRENT_SOURCE_DIR}/CMakeLists.txt
  ${CMAKE_CURRENT_SOURCE_DIR}/aub_command_stream_receiver.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/aub_command_stream_receiver.h
  ${CMAKE_CURRENT_SOURCE_DIR}/aub_command_stream_receiver_hw.h
  ${CMAKE_CURRENT_SOURCE_DIR}/aub_command_stream_receiver_hw.inl
  ${CMAKE_CURRENT_SOURCE_DIR}/aub_subcapture.h
  ${CMAKE_CURRENT_SOURCE_DIR}/aub_subcapture.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/command_stream_receiver.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/command_stream_receiver.h
  ${CMAKE_CURRENT_SOURCE_DIR}/command_stream_receiver_hw.h
  ${CMAKE_CURRENT_SOURCE_DIR}/command_stream_receiver_hw.inl
  ${CMAKE_CURRENT_SOURCE_DIR}/command_stream_receiver_with_aub_dump.h
  ${CMAKE_CURRENT_SOURCE_DIR}/command_stream_receiver_with_aub_dump.inl
  ${CMAKE_CURRENT_SOURCE_DIR}/create_command_stream_impl.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/create_command_stream_impl.h
  ${CMAKE_CURRENT_SOURCE_DIR}/csr_definitions.h
  ${CMAKE_CURRENT_SOURCE_DIR}/device_command_stream.h
  ${CMAKE_CURRENT_SOURCE_DIR}/linear_stream.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/linear_stream.h
  ${CMAKE_CURRENT_SOURCE_DIR}/submissions_aggregator.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/submissions_aggregator.h
  ${CMAKE_CURRENT_SOURCE_DIR}/tbx_command_stream_receiver.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/tbx_command_stream_receiver.h
  ${CMAKE_CURRENT_SOURCE_DIR}/tbx_command_stream_receiver_hw.h
  ${CMAKE_CURRENT_SOURCE_DIR}/tbx_command_stream_receiver_hw.inl
  ${CMAKE_CURRENT_SOURCE_DIR}/tbx_stream.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/thread_arbitration_policy.h
  ${CMAKE_CURRENT_SOURCE_DIR}/preemption.h
  ${CMAKE_CURRENT_SOURCE_DIR}/preemption.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/preemption.inl
)
target_sources(${NEO_STATIC_LIB_NAME} PRIVATE ${RUNTIME_SRCS_COMMAND_STREAM})
set_property(GLOBAL PROPERTY RUNTIME_SRCS_COMMAND_STREAM ${RUNTIME_SRCS_COMMAND_STREAM})
