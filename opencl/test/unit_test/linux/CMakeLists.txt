#
# Copyright (C) 2018-2021 Intel Corporation
#
# SPDX-License-Identifier: MIT
#

set(IGDRCL_SRCS_linux_dll_tests
)

set(target_name linux_dll_tests)
add_executable(igdrcl_${target_name}
               $<TARGET_OBJECTS:igdrcl_libult>
               $<TARGET_OBJECTS:igdrcl_libult_common>
               $<TARGET_OBJECTS:igdrcl_libult_cs>
               $<TARGET_OBJECTS:mock_gmm>
               ${CMAKE_CURRENT_SOURCE_DIR}/drm_null_device_tests.cpp
               ${CMAKE_CURRENT_SOURCE_DIR}/drm_wrap.h
               ${CMAKE_CURRENT_SOURCE_DIR}/linux_tests_configuration.cpp
               ${CMAKE_CURRENT_SOURCE_DIR}/main_linux_dll.cpp
               ${CMAKE_CURRENT_SOURCE_DIR}/mock_os_layer.cpp
               ${CMAKE_CURRENT_SOURCE_DIR}/mock_os_layer.h
               ${CMAKE_CURRENT_SOURCE_DIR}/os_interface_linux_tests.cpp
               ${CMAKE_CURRENT_SOURCE_DIR}/test_mode.h
               ${NEO_SHARED_DIRECTORY}/os_interface/linux/sys_calls_linux.cpp
               ${NEO_SHARED_TEST_DIRECTORY}/common/aub_stream_mocks/aub_stream_interface_mock.cpp
               ${NEO_SOURCE_DIR}/opencl/source/dll/linux/allocator_helper.cpp
               ${NEO_SOURCE_DIR}/opencl/source/dll/linux/drm_neo_create.cpp
               ${NEO_SOURCE_DIR}/opencl/source/dll/linux/options_linux.cpp
               ${NEO_SOURCE_DIR}/opencl/source/dll/linux/os_interface.cpp
               ${NEO_SOURCE_DIR}/opencl/source/helpers/api_specific_config_ocl.cpp
               ${NEO_SOURCE_DIR}/opencl/source/os_interface/linux/platform_teardown_linux.cpp
               ${NEO_SOURCE_DIR}/opencl/test/unit_test/linux${BRANCH_DIR_SUFFIX}/drm_other_requests.cpp
               ${NEO_SOURCE_DIR}/opencl/test/unit_test/os_interface/linux/create_drm_memory_manager.cpp
               ${NEO_SOURCE_DIR}/opencl/test/unit_test/os_interface/linux/device_command_stream_fixture.cpp
)
if(NEO__LIBVA_FOUND)
  target_sources(igdrcl_${target_name} PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/va_tests.cpp)
endif()

if(NOT DISABLE_WDDM_LINUX)
  target_sources(igdrcl_${target_name} PRIVATE ${NEO_SHARED_DIRECTORY}/os_interface/windows/wddm/wddm_create.cpp
                 ${NEO_SHARED_DIRECTORY}/gmm_helper/windows/gmm_memory.cpp
  )
endif()

target_link_libraries(igdrcl_${target_name} ${NEO_STATICALLY_LINKED_LIBRARIES_MOCKABLE} igdrcl_mocks ${NEO_STATICALLY_LINKED_LIBRARIES_MOCKABLE} ${IGDRCL_EXTRA_LIBS} gmock-gtest)

set_property(TARGET igdrcl_${target_name} APPEND_STRING PROPERTY COMPILE_FLAGS ${ASAN_FLAGS})
target_include_directories(igdrcl_${target_name} PRIVATE
                           ${NEO_SHARED_TEST_DIRECTORY}/common/test_macros/header${BRANCH_DIR_SUFFIX}
                           ${NEO_SOURCE_DIR}/opencl/source/dll/linux/devices${BRANCH_DIR_SUFFIX}
                           ${NEO_SOURCE_DIR}/opencl/test/unit_test/gen_common${BRANCH_DIR_SUFFIX}
                           ${CMAKE_CURRENT_SOURCE_DIR}
)
add_dependencies(unit_tests igdrcl_${target_name})
add_subdirectories()
