#
# Copyright (C) 2018-2025 Intel Corporation
#
# SPDX-License-Identifier: MIT
#

if(WIN32)
  project(igdrcl_windows_dll_tests)

  set(NEO_IGDRCL_WINDOWS_DLL_TESTS_TARGET_OBJECTS
      $<TARGET_OBJECTS:neo_libult_common>
      $<TARGET_OBJECTS:neo_shared_mocks>
      $<TARGET_OBJECTS:neo_unit_tests_config>
      $<TARGET_OBJECTS:igdrcl_libult>
      $<TARGET_OBJECTS:igdrcl_libult_env>
      $<TARGET_OBJECTS:mock_aubstream>
      $<TARGET_OBJECTS:mock_gmm>
  )

  add_executable(igdrcl_windows_dll_tests
                 ${CMAKE_CURRENT_SOURCE_DIR}${BRANCH_DIR_SUFFIX}get_devices_tests.cpp
                 ${CMAKE_CURRENT_SOURCE_DIR}/wddm_create_tests.cpp
                 ${NEO_IGDRCL_WINDOWS_DLL_TESTS_TARGET_OBJECTS}
                 ${NEO_SHARED_DIRECTORY}/dll/device_dll.cpp
                 ${NEO_SHARED_DIRECTORY}/dll/direct_submission_controller_enabled.cpp
                 ${NEO_SHARED_DIRECTORY}/dll/get_devices.cpp
                 ${NEO_SHARED_DIRECTORY}/dll/unified_memory_reuse_cleaner_enabled.cpp
                 ${NEO_SHARED_DIRECTORY}/dll/create_command_stream.cpp
                 ${NEO_SHARED_DIRECTORY}/dll/wddm/wddm_create.cpp
                 ${NEO_SHARED_TEST_DIRECTORY}/common/mocks/mock_wddm.cpp
                 ${NEO_SHARED_TEST_DIRECTORY}/common/os_interface/windows/create_wddm_memory_manager.cpp
                 ${NEO_SHARED_TEST_DIRECTORY}/common/os_interface/windows${BRANCH_DIR_SUFFIX}/options.cpp
                 ${NEO_SHARED_TEST_DIRECTORY}/common/os_interface/windows/options.inl
                 ${NEO_SHARED_TEST_DIRECTORY}/common/os_interface/setup_external_dependencies_drm_or_wddm.cpp
                 ${NEO_SHARED_TEST_DIRECTORY}/common/os_interface/windows/sys_calls.cpp
                 ${NEO_SHARED_TEST_DIRECTORY}/common/os_interface/windows/sys_calls_winmm.cpp
                 ${NEO_SHARED_TEST_DIRECTORY}/common/os_interface/windows/wddm_calls.cpp
                 ${NEO_SHARED_TEST_DIRECTORY}/common/mocks/mock_get_staging_buffer_size.cpp
                 ${NEO_SHARED_TEST_DIRECTORY}/common/mocks/mock_usm_pool_size.cpp
                 ${NEO_SOURCE_DIR}/opencl/test/unit_test/test_macros/test_checks_ocl.cpp
  )

  target_link_libraries(igdrcl_windows_dll_tests ${NEO_MOCKABLE_LIB_NAME} ${NEO_SHARED_MOCKABLE_LIB_NAME} igdrcl_mocks gmock-gtest ${NEO_EXTRA_LIBS})

  target_include_directories(igdrcl_windows_dll_tests PRIVATE
                             ${NEO_SHARED_TEST_DIRECTORY}/common/test_configuration/unit_tests
                             ${NEO_SHARED_TEST_DIRECTORY}/common/test_macros/header${BRANCH_DIR_SUFFIX}
                             ${NEO_SOURCE_DIR}/opencl/test/unit_test/gen_common${BRANCH_DIR_SUFFIX}
  )
  add_dependencies(unit_tests igdrcl_windows_dll_tests)

  create_project_source_tree(igdrcl_windows_dll_tests)

  add_custom_target(run_windows_dll_tests ALL DEPENDS unit_tests)
  add_custom_command(
                     TARGET run_windows_dll_tests
                     POST_BUILD
                     COMMAND WORKING_DIRECTORY ${TargetDir}
                     COMMAND echo Target Directory is: ${TargetDir}
                     COMMAND echo Running Windows dll tests
                     COMMAND igdrcl_windows_dll_tests --slices 1 --subslices 2 --eu_per_ss 4
  )
  add_dependencies(run_ocl_tests run_windows_dll_tests)

  set_target_properties(igdrcl_windows_dll_tests PROPERTIES FOLDER ${OPENCL_TEST_PROJECTS_FOLDER})
  set_target_properties(run_windows_dll_tests PROPERTIES FOLDER ${OPENCL_TEST_PROJECTS_FOLDER})
endif()
