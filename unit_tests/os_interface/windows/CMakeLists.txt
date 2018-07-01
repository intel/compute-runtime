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

set(IGDRCL_SRCS_tests_os_interface_windows
  ${CMAKE_CURRENT_SOURCE_DIR}/CMakeLists.txt
  ${CMAKE_CURRENT_SOURCE_DIR}/deferrable_deletion_win_tests.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/device_command_stream_tests.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/device_os_tests.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/driver_info_tests.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/gdi_dll_fixture.h
  ${CMAKE_CURRENT_SOURCE_DIR}/gdi_interface_tests.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/hw_info_config_win_tests.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/hw_info_config_win_tests.h
  ${CMAKE_CURRENT_SOURCE_DIR}/mock_gdi_interface.h
  ${CMAKE_CURRENT_SOURCE_DIR}/mock_kmdaf_listener.h
  ${CMAKE_CURRENT_SOURCE_DIR}/mock_os_time_win.h
  ${CMAKE_CURRENT_SOURCE_DIR}/mock_performance_counters_win.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/mock_performance_counters_win.h
  ${CMAKE_CURRENT_SOURCE_DIR}/mock_wddm_memory_manager.h
  ${CMAKE_CURRENT_SOURCE_DIR}/os_interface_win_tests.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/os_interface_win_tests.h
  ${CMAKE_CURRENT_SOURCE_DIR}/os_library_win_tests.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/os_time_win_tests.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/performance_counters_win_tests.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/registry_reader_tests.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/self_lib_win.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/wddm_address_space_tests.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/wddm_fixture.h
  ${CMAKE_CURRENT_SOURCE_DIR}/wddm_kmdaf_listener_tests.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/wddm_memory_manager_tests.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/wddm_memory_manager_tests.h
  ${CMAKE_CURRENT_SOURCE_DIR}/wddm_preemption_tests.cpp
)
if(WIN32)
  file(GLOB IGDRCL_SRC_tests_wddm_interface "${CMAKE_CURRENT_SOURCE_DIR}/wddm2[0-9]_tests\.cpp")
  target_sources(igdrcl_tests PRIVATE
    ${IGDRCL_SRCS_tests_os_interface_windows}
    ${IGDRCL_SRC_tests_wddm_interface}
  )
endif()
add_subdirectories()
