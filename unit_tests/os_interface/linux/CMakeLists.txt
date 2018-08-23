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

set(IGDRCL_SRCS_tests_os_interface_linux
  ${CMAKE_CURRENT_SOURCE_DIR}/CMakeLists.txt
  ${CMAKE_CURRENT_SOURCE_DIR}/allocator_helper_tests.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/debug_env_reader.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/device_command_stream_tests.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/device_factory_tests.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/device_factory_tests.h
  ${CMAKE_CURRENT_SOURCE_DIR}/device_os_tests.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/driver_info_tests.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/drm_buffer_object_tests.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/drm_command_stream_mm_tests.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/drm_command_stream_tests.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/drm_gem_close_worker_tests.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/drm_memory_manager_tests.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/drm_mock.h
  ${CMAKE_CURRENT_SOURCE_DIR}/drm_neo_create.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/drm_tests.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/hw_info_config_linux_tests.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/hw_info_config_linux_tests.h
  ${CMAKE_CURRENT_SOURCE_DIR}/mock_os_time_linux.h
  ${CMAKE_CURRENT_SOURCE_DIR}/mock_performance_counters_linux.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/mock_performance_counters_linux.h
  ${CMAKE_CURRENT_SOURCE_DIR}/os_context_linux_tests.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/os_interface_linux_tests.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/os_time_test.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/performance_counters_linux_tests.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/self_lib_lin.cpp
)
if(UNIX)
  target_sources(igdrcl_tests PRIVATE ${IGDRCL_SRCS_tests_os_interface_linux})
endif()
set_property(GLOBAL PROPERTY IGDRCL_SRCS_tests_os_interface_linux ${IGDRCL_SRCS_tests_os_interface_linux})
add_subdirectories()
