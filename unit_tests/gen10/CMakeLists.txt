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

if(TESTS_GEN10)
  set(IGDRCL_SRCS_tests_gen10
    ${CMAKE_CURRENT_SOURCE_DIR}/CMakeLists.txt
    ${CMAKE_CURRENT_SOURCE_DIR}/coherency_tests_gen10.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/command_stream_receiver_hw_tests_gen10.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/enqueue_kernel_gen10.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/enqueue_media_kernel_gen10.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/hw_helper_tests_gen10.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/image_tests_gen10.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/kernel_tests_gen10.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/preamble_tests_gen10.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/sampler_tests_gen10.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/scheduler_source_tests_gen10.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/test_device_caps_gen10.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/test_device_queue_hw_gen10.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/test_platform_caps_gen10.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/test_preamble_gen10.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/test_preemption_gen10.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/test_sample_gen10.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/unit_test_helper_gen10.cpp
  )
  target_sources(igdrcl_tests PRIVATE ${IGDRCL_SRCS_tests_gen10})
  add_subdirectories()
endif()
