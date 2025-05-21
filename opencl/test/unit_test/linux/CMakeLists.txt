#
# Copyright (C) 2018-2025 Intel Corporation
#
# SPDX-License-Identifier: MIT
#

set(IGDRCL_SRCS_linux_dll_tests
)

set(target_name linux_dll_tests)
add_executable(igdrcl_${target_name}
               $<TARGET_OBJECTS:igdrcl_libult>
               $<TARGET_OBJECTS:neo_libult_common>
               $<TARGET_OBJECTS:neo_libult_cs>
               $<TARGET_OBJECTS:neo_shared_mocks>
               $<TARGET_OBJECTS:neo_unit_tests_config>
               $<TARGET_OBJECTS:mock_aubstream>
               $<TARGET_OBJECTS:mock_gmm>
               ${CMAKE_CURRENT_SOURCE_DIR}/drm_wrap.h
               ${CMAKE_CURRENT_SOURCE_DIR}/main_linux_dll.cpp
               ${CMAKE_CURRENT_SOURCE_DIR}/mock_os_layer.cpp
               ${CMAKE_CURRENT_SOURCE_DIR}/mock_os_layer.h
               ${CMAKE_CURRENT_SOURCE_DIR}/os_interface_linux_tests.cpp
               ${NEO_SHARED_TEST_DIRECTORY}/common/helpers/virtual_file_system_listener.cpp
               ${NEO_SHARED_TEST_DIRECTORY}/common/mocks/mock_get_staging_buffer_size.cpp
               ${NEO_SHARED_DIRECTORY}/os_interface/linux/sys_calls_linux.cpp
               ${NEO_SHARED_DIRECTORY}/dll/create_memory_manager_drm.cpp
               ${NEO_SHARED_DIRECTORY}/dll/device_dll.cpp
               ${NEO_SHARED_DIRECTORY}/dll/direct_submission_controller_enabled.cpp
               ${NEO_SHARED_DIRECTORY}/dll/unified_memory_reuse_cleaner_enabled.cpp
               ${NEO_SHARED_DIRECTORY}/dll/linux/drm_neo_create.cpp
               ${NEO_SHARED_DIRECTORY}/dll/linux${BRANCH_DIR_SUFFIX}/options_linux.cpp
               ${NEO_SHARED_DIRECTORY}/dll/linux/options_linux.inl
               ${NEO_SHARED_DIRECTORY}/dll/linux/os_interface.cpp
               ${NEO_SOURCE_DIR}/opencl/source/dll/command_queue_dll.cpp
               ${NEO_SOURCE_DIR}/opencl/source/os_interface/linux/platform_teardown_linux.cpp
)
if(NEO__LIBVA_FOUND)
  target_sources(igdrcl_${target_name} PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/va_tests.cpp)
endif()
if(NOT DISABLE_WDDM_LINUX)
  target_sources(igdrcl_${target_name} PRIVATE ${NEO_SHARED_DIRECTORY}/dll/wddm/wddm_create.cpp)
endif()

target_link_libraries(igdrcl_${target_name} ${NEO_MOCKABLE_LIB_NAME} ${NEO_SHARED_MOCKABLE_LIB_NAME} igdrcl_mocks ${NEO_EXTRA_LIBS} gmock-gtest)

set_property(TARGET igdrcl_${target_name} APPEND_STRING PROPERTY COMPILE_FLAGS ${ASAN_FLAGS})
target_include_directories(igdrcl_${target_name} PRIVATE
                           ${NEO_SHARED_TEST_DIRECTORY}/common/test_macros/header${BRANCH_DIR_SUFFIX}
                           ${NEO_SHARED_DIRECTORY}/dll/linux/devices${BRANCH_DIR_SUFFIX}
                           ${NEO_SOURCE_DIR}/opencl/test/unit_test/gen_common${BRANCH_DIR_SUFFIX}
                           ${NEO_SHARED_TEST_DIRECTORY}/common/test_configuration/unit_tests
)
add_dependencies(unit_tests igdrcl_${target_name})
add_subdirectories()
