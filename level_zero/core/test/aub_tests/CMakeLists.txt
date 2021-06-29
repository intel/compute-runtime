#
# Copyright (C) 2021 Intel Corporation
#
# SPDX-License-Identifier: MIT
#

project(ze_intel_gpu_aub_tests)

set(L0_AUB_TEST_DIR ${CMAKE_CURRENT_SOURCE_DIR})

get_property(NEO_SHARED_aub_tests_configurations GLOBAL PROPERTY NEO_SHARED_aub_tests_configurations)

list(APPEND L0_AUB_TESTS__TARGET_OBJECTS
     ${CMAKE_CURRENT_SOURCE_DIR}/aub_hello_world_test.cpp
     ${CMAKE_CURRENT_SOURCE_DIR}/test_mode.h
     ${NEO_SOURCE_DIR}/level_zero/core/test/unit_tests/main.cpp
     ${NEO_SHARED_aub_tests_configurations}
     ${NEO_SHARED_TEST_DIRECTORY}/unit_test/tests_configuration.h
)

add_executable(ze_intel_gpu_aub_tests
               ${L0_AUB_TESTS__TARGET_OBJECTS}
)

target_sources(ze_intel_gpu_aub_tests PRIVATE
               ${CMAKE_CURRENT_SOURCE_DIR}/CMakeLists.txt
               ${L0_CORE_ENABLERS}
               ${NEO_SOURCE_DIR}/level_zero/core/test/unit_tests/main.cpp
               ${NEO_SOURCE_DIR}/level_zero/core/test/unit_tests/mock.h
               ${NEO_SOURCE_DIR}/level_zero/core/test/unit_tests/white_box.h
               ${NEO_SOURCE_DIR}/level_zero/tools/test/unit_tests/sources/debug/debug_session_helper.cpp
               ${NEO_SOURCE_DIR}/level_zero/core/source/dll/create_builtin_functions_lib.cpp
               ${NEO_SOURCE_DIR}/level_zero/core/source/dll/disallow_deferred_deleter.cpp
)

target_sources(ze_intel_gpu_aub_tests PRIVATE
               ${NEO_SHARED_TEST_DIRECTORY}/common/libult/os_interface.cpp
               ${NEO_SHARED_TEST_DIRECTORY}/common/mocks/ult_device_factory.cpp
               ${NEO_SHARED_TEST_DIRECTORY}/common/mocks/ult_device_factory.h
               ${NEO_SHARED_TEST_DIRECTORY}/common/mocks${BRANCH_SUFIX_DIR}/mock_gmm_client_context.cpp
               ${NEO_SHARED_TEST_DIRECTORY}/common/mocks/mock_cif.cpp
               ${NEO_SHARED_TEST_DIRECTORY}/common/mocks/mock_command_stream_receiver.cpp
               ${NEO_SHARED_TEST_DIRECTORY}/common/mocks/mock_compiler_interface_spirv.cpp
               ${NEO_SHARED_TEST_DIRECTORY}/common/mocks/mock_compiler_interface_spirv.h
               ${NEO_SHARED_TEST_DIRECTORY}/common/mocks/mock_compilers.cpp
               ${NEO_SHARED_TEST_DIRECTORY}/common/mocks/mock_deferred_deleter.cpp
               ${NEO_SHARED_TEST_DIRECTORY}/common/mocks/mock_device.cpp
               ${NEO_SHARED_TEST_DIRECTORY}/common/mocks/mock_gmm_client_context_base.cpp
               ${NEO_SHARED_TEST_DIRECTORY}/common/mocks/mock_sip.cpp
               ${NEO_SHARED_DIRECTORY}/debug_settings/debug_settings_manager.cpp
               ${NEO_SHARED_DIRECTORY}/aub/aub_stream_interface.cpp
)

target_sources(ze_intel_gpu_aub_tests PRIVATE
               $<TARGET_OBJECTS:${L0_MOCKABLE_LIB_NAME}>
               $<TARGET_OBJECTS:${TARGET_NAME_L0}_fixtures>
               $<TARGET_OBJECTS:${TARGET_NAME_L0}_mocks>
               $<TARGET_OBJECTS:igdrcl_libult_common>
               $<TARGET_OBJECTS:${BUILTINS_SPIRV_LIB_NAME}>
               $<TARGET_OBJECTS:${BUILTINS_BINARIES_BINDFUL_LIB_NAME}>
               $<TARGET_OBJECTS:${BUILTINS_BINARIES_BINDLESS_LIB_NAME}>
)

target_sources(ze_intel_gpu_aub_tests PRIVATE
               ${NEO_SOURCE_DIR}/opencl/test/unit_test/helpers${BRANCH_DIR_SUFFIX}/kernel_binary_helper_hash_value.cpp
               ${NEO_SOURCE_DIR}/opencl/test/unit_test/libult/create_command_stream.cpp
               ${NEO_SOURCE_DIR}/opencl/test/unit_test/libult/io_functions.cpp
               ${NEO_SOURCE_DIR}/opencl/test/unit_test/global_environment.cpp
               ${NEO_SOURCE_DIR}/opencl/test/unit_test/abort.cpp
               ${NEO_SOURCE_DIR}/opencl/test/unit_test/mocks/mock_gmm_page_table_mngr.cpp
               ${NEO_SOURCE_DIR}/opencl/test/unit_test/mocks/mock_memory_manager.cpp
               ${NEO_SOURCE_DIR}/opencl/test/unit_test/utilities/debug_settings_reader_creator.cpp
)

if(UNIX)
  if(NOT DISABLE_WDDM_LINUX)
    target_sources(ze_intel_gpu_aub_tests PRIVATE
                   ${NEO_SHARED_DIRECTORY}/gmm_helper/windows/gmm_memory.cpp
                   ${NEO_SHARED_DIRECTORY}/os_interface/windows/wddm/wddm_create.cpp
    )
  endif()
  target_sources(ze_intel_gpu_aub_tests PRIVATE
                 ${NEO_SOURCE_DIR}/opencl/test/unit_test/os_interface/linux/create_drm_memory_manager.cpp
                 ${NEO_SOURCE_DIR}/opencl/test/unit_test/os_interface/linux/drm_neo_create.cpp
                 ${NEO_SOURCE_DIR}/opencl/test/unit_test/os_interface/linux/options.cpp
                 ${NEO_SOURCE_DIR}/opencl/test/unit_test/os_interface/linux/sys_calls_linux_ult.cpp
                 ${NEO_SOURCE_DIR}/opencl/test/unit_test/os_interface/linux/drm_mock.cpp
                 ${NEO_SOURCE_DIR}/opencl/test/unit_test/os_interface/linux/allocator_helper.cpp
                 ${NEO_SHARED_DIRECTORY}/os_interface/linux/gmm_interface_linux.cpp
  )
else()
  target_sources(ze_intel_gpu_aub_tests PRIVATE
                 ${NEO_SHARED_DIRECTORY}/gmm_helper/windows/gmm_memory.cpp
                 ${NEO_SHARED_DIRECTORY}/os_interface/windows/gmm_interface_win.cpp
                 ${NEO_SHARED_DIRECTORY}/dll/windows/environment_variables.cpp
                 ${NEO_SOURCE_DIR}/shared/source/dll/windows/environment_variables.cpp
                 ${NEO_SOURCE_DIR}/opencl/test/unit_test/os_interface/windows/create_wddm_memory_manager.cpp
                 ${NEO_SOURCE_DIR}/opencl/test/unit_test/os_interface/windows/options.cpp
                 ${NEO_SOURCE_DIR}/opencl/test/unit_test/os_interface/windows/sys_calls.cpp
                 ${NEO_SOURCE_DIR}/opencl/test/unit_test/os_interface/windows/ult_dxcore_factory.cpp
                 ${NEO_SOURCE_DIR}/opencl/test/unit_test/os_interface/windows/ult_dxgi_factory.cpp
                 ${NEO_SOURCE_DIR}/opencl/test/unit_test/os_interface/windows/wddm_calls.cpp
                 ${NEO_SOURCE_DIR}/opencl/test/unit_test/os_interface/windows/wddm_create.cpp
                 ${NEO_SOURCE_DIR}/opencl/test/unit_test/mocks/mock_wddm.cpp
  )
endif()

copy_gmm_dll_for(ze_intel_gpu_aub_tests)
add_subdirectories()

target_link_libraries(ze_intel_gpu_aub_tests ${NEO_STATICALLY_LINKED_LIBRARIES_MOCKABLE})
target_link_libraries(ze_intel_gpu_aub_tests ${HW_LIBS_ULT})
target_link_libraries(ze_intel_gpu_aub_tests gmock-gtest)
target_link_libraries(ze_intel_gpu_aub_tests ${IGDRCL_EXTRA_LIBS})

if(UNIX)
  target_link_libraries(ze_intel_gpu_aub_tests rt)
  target_link_libraries(ze_intel_gpu_aub_tests ${GMM_LINK_NAME})
else()
  target_link_libraries(ze_intel_gpu_aub_tests dbghelp)
  add_dependencies(ze_intel_gpu_aub_tests ${GMM_TARGET_NAME})
endif()

target_include_directories(ze_intel_gpu_aub_tests PRIVATE
                           ${CMAKE_CURRENT_SOURCE_DIR}
                           ${NEO_SOURCE_DIR}/shared/test/common/helpers/includes${BRANCH_DIR_SUFFIX}
                           ${NEO_SHARED_TEST_DIRECTORY}/common/test_macros/header${BRANCH_DIR_SUFFIX}
                           ${NEO_SOURCE_DIR}/opencl/test/unit_test/mocks/gmm_memory
                           ${NEO_SOURCE_DIR}/level_zero/core/test/unit_tests
                           ${ENGINE_NODE_DIR}
                           ${CIF_BASE_DIR}
                           ${NEO__GMM_INCLUDE_DIR}
                           ${THIRD_PARTY_DIR}
                           ${WDK_DIR}
)

create_project_source_tree(ze_intel_gpu_aub_tests)
