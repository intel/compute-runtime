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

if(MSVC)
set(IGDRCL_SRCS_tests_os_interface_windows
    "${CMAKE_CURRENT_SOURCE_DIR}/windows/deferrable_deletion_win_tests.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/windows/device_command_stream_tests.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/windows/device_os_tests.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/windows/driver_info_tests.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/windows/gdi_interface_tests.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/windows/mock_kmdaf_listener.h"
    "${CMAKE_CURRENT_SOURCE_DIR}/windows/mock_gdi_interface.h"
    "${CMAKE_CURRENT_SOURCE_DIR}/windows/mock_os_time_win.h"
    "${CMAKE_CURRENT_SOURCE_DIR}/windows/mock_wddm_memory_manager.h"
    "${CMAKE_CURRENT_SOURCE_DIR}/windows/options.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/windows/os_time_win_tests.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/windows/os_interface_tests.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/windows/os_interface_tests.h"
    "${CMAKE_CURRENT_SOURCE_DIR}/windows/self_lib_win.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/windows/ult_dxgi_factory.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/windows/ult_dxgi_factory.h"
    "${CMAKE_CURRENT_SOURCE_DIR}/windows/wddm_create.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/windows/wddm_fixture.h"
    "${CMAKE_CURRENT_SOURCE_DIR}/windows/wddm_kmdaf_listener_tests.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/windows/wddm_memory_manager_tests.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/windows/wddm_memory_manager_tests.h"
    "${CMAKE_CURRENT_SOURCE_DIR}/windows/wddm_tests.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/windows/wddm_tests.h"
)

set(IGDRCL_SRCS_tests_os_interface_windows_perf_counters
    "${CMAKE_CURRENT_SOURCE_DIR}/windows/mock_performance_counters_win.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/windows/mock_performance_counters_win.h"
    "${CMAKE_CURRENT_SOURCE_DIR}/windows/performance_counters_win_tests.cpp"
)

set(IGDRCL_SRCS_perf_tests_os_interface
    "${CMAKE_CURRENT_SOURCE_DIR}/windows/options.cpp"
    PARENT_SCOPE
)

elseif(UNIX)

set(IGDRCL_SRCS_tests_os_interface_linux
    "${CMAKE_CURRENT_SOURCE_DIR}/linux/debug_env_reader.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/linux/device_command_stream_tests.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/linux/device_os_tests.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/linux/driver_info_tests.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/linux/drm_command_stream_tests.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/linux/drm_command_stream_mm_tests.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/linux/drm_buffer_object_tests.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/linux/drm_gem_close_worker_tests.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/linux/drm_memory_manager_tests.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/linux/drm_mock.h"
    "${CMAKE_CURRENT_SOURCE_DIR}/linux/drm_neo_create.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/linux/drm_tests.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/linux/device_factory_tests.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/linux/device_factory_tests.h"
    "${CMAKE_CURRENT_SOURCE_DIR}/linux/hw_info_config_tests.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/linux/hw_info_config_tests.h"
    "${CMAKE_CURRENT_SOURCE_DIR}/linux/mock_os_time_linux.h"
    "${CMAKE_CURRENT_SOURCE_DIR}/linux/options.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/linux/os_time_test.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/linux/self_lib_lin.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/linux/os_interface_tests.cpp"
)

set(IGDRCL_SRCS_tests_os_interface_linux_perf_counters
    "${CMAKE_CURRENT_SOURCE_DIR}/linux/mock_performance_counters_linux.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/linux/mock_performance_counters_linux.h"
    "${CMAKE_CURRENT_SOURCE_DIR}/linux/performance_counters_linux_tests.cpp"
)

set(IGDRCL_SRCS_perf_tests_os_interface
    "${CMAKE_CURRENT_SOURCE_DIR}/linux/options.cpp"
    PARENT_SCOPE
)

endif()

# We are setting all of the source/OpenCL/unit_tests/os_interface files here and
# sending the variable up to the parent scope

set(IGDRCL_SRCS_tests_os_interface_perf_counters
    "${CMAKE_CURRENT_SOURCE_DIR}/mock_performance_counters.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/mock_performance_counters.h"
    "${CMAKE_CURRENT_SOURCE_DIR}/performance_counters_tests.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/performance_counters_gen_tests.cpp"
)

set(IGDRCL_SRCS_tests_os_interface
    "${CMAKE_CURRENT_SOURCE_DIR}/CMakeLists.txt"
    "${CMAKE_CURRENT_SOURCE_DIR}/32bit_memory_tests.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/debug_settings_manager_tests.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/device_factory_tests.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/hw_info_config_tests.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/hw_info_config_tests.h"
    "${CMAKE_CURRENT_SOURCE_DIR}/os_library_tests.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/os_interface_tests.cpp"
    "${IGDRCL_SRCS_tests_os_interface_linux}"
    "${IGDRCL_SRCS_tests_os_interface_windows}"
    "${IGDRCL_SRCS_tests_os_interface_perf_counters}"
    "${IGDRCL_SRCS_tests_os_interface_linux_perf_counters}"
    "${IGDRCL_SRCS_tests_os_interface_windows_perf_counters}"
    PARENT_SCOPE
)
