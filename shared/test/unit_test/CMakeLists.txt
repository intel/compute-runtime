#
# Copyright (C) 2019-2020 Intel Corporation
#
# SPDX-License-Identifier: MIT
#

if(NOT SKIP_NEO_UNIT_TESTS AND NOT SKIP_UNIT_TESTS)

  set(TARGET_NAME neo_shared_tests)

  # disable optimizations for ults
  if(UNIX)
    string(REPLACE "-O2" "" CMAKE_CXX_FLAGS ${CMAKE_CXX_FLAGS})
    #disable fortify source as this requires optimization to be on
    string(REPLACE "-Wp,-D_FORTIFY_SOURCE=2" "" CMAKE_CXX_FLAGS ${CMAKE_CXX_FLAGS})
    string(REPLACE "-D_FORTIFY_SOURCE=2" "" CMAKE_CXX_FLAGS ${CMAKE_CXX_FLAGS})
    set(CMAKE_CXX_FLAGS_RELEASE "${CMAKE_CXX_FLAGS_RELEASE} -O0")
    set(CMAKE_CXX_FLAGS_RELEASEINTERNAL "${CMAKE_CXX_FLAGS_RELEASEINTERNAL} -O0")
    set(CMAKE_C_FLAGS_RELEASE "${CMAKE_C_FLAGS_RELEASE} -O0")
    set(CMAKE_C_FLAGS_RELEASEINTERNAL "${CMAKE_C_FLAGS_RELEASEINTERNAL} -O0")
  endif()

  if(WIN32)
    string(REPLACE "/O2" "" CMAKE_CXX_FLAGS ${CMAKE_CXX_FLAGS})
    string(REPLACE "/O2" "/Od" CMAKE_CXX_FLAGS_RELEASE ${CMAKE_CXX_FLAGS_RELEASE})
    string(REPLACE "/O2" "/Od" CMAKE_C_FLAGS_RELEASE ${CMAKE_C_FLAGS_RELEASE})
    string(REPLACE "/O2" "/Od" CMAKE_CXX_FLAGS_RELEASEINTERNAL ${CMAKE_CXX_FLAGS_RELEASEINTERNAL})
    string(REPLACE "/O2" "/Od" CMAKE_C_FLAGS_RELEASEINTERNAL ${CMAKE_C_FLAGS_RELEASEINTERNAL})
  endif()

  function(ADD_SUPPORTED_TEST_PRODUCT_FAMILIES_DEFINITION)
    set(NEO_SUPPORTED_PRODUCT_FAMILIES ${ALL_PRODUCT_FAMILY_LIST})
    string(REPLACE ";" "," NEO_SUPPORTED_PRODUCT_FAMILIES "${NEO_SUPPORTED_PRODUCT_FAMILIES}")
    add_definitions(-DSUPPORTED_TEST_PRODUCT_FAMILIES=${NEO_SUPPORTED_PRODUCT_FAMILIES})
  endfunction()

  ADD_SUPPORTED_TEST_PRODUCT_FAMILIES_DEFINITION()
  link_libraries(${ASAN_LIBS} ${TSAN_LIBS})

  append_sources_from_properties(CORE_ENABLERS NEO_CORE_SRCS_LINK)

  add_executable(${TARGET_NAME}
                 ${CMAKE_CURRENT_SOURCE_DIR}/CMakeLists.txt
                 ${CMAKE_CURRENT_SOURCE_DIR}/main.cpp
                 ${CMAKE_CURRENT_SOURCE_DIR}/test_mode.h
                 ${CMAKE_CURRENT_SOURCE_DIR}/tests_configuration.h
                 ${CMAKE_CURRENT_SOURCE_DIR}/mocks/mock_dispatch_kernel_encoder_interface.cpp
                 ${CMAKE_CURRENT_SOURCE_DIR}/mocks/mock_dispatch_kernel_encoder_interface.h
                 ${CORE_ENABLERS}
                 ${NEO_SOURCE_DIR}/opencl/source/compiler_interface/default_cache_config.cpp
                 ${NEO_SOURCE_DIR}/opencl/test/unit_test/fixtures/mock_execution_environment_gmm_fixture.cpp
                 ${NEO_SOURCE_DIR}/opencl/test/unit_test/global_environment.cpp
                 ${NEO_SOURCE_DIR}/opencl/test/unit_test/helpers/kernel_binary_helper_hash_value.cpp
                 ${NEO_SOURCE_DIR}/opencl/test/unit_test/libult/create_command_stream.cpp
                 ${NEO_SOURCE_DIR}/opencl/test/unit_test/libult/io_functions.cpp
                 ${NEO_SOURCE_DIR}/opencl/test/unit_test/mocks/mock_platform.cpp
                 ${NEO_SOURCE_DIR}/opencl/test/unit_test/ult_configuration.cpp
                 ${NEO_SOURCE_DIR}/opencl/test/unit_test/ult_config_listener.cpp
                 ${NEO_SOURCE_DIR}/shared/source/helpers/allow_deferred_deleter.cpp
                 ${NEO_SOURCE_DIR}/shared/test/unit_test/helpers/api_specific_config_shared_tests.cpp
                 ${NEO_SOURCE_DIR}/shared/test/unit_test/helpers/memory_management.cpp
                 ${NEO_SOURCE_DIR}/shared/test/unit_test/helpers/memory_leak_listener.cpp
                 $<TARGET_OBJECTS:mock_gmm>
  )

  if(UNIX)
    target_sources(${TARGET_NAME} PRIVATE
                   ${NEO_SOURCE_DIR}/opencl/source/dll/linux/os_interface.cpp
                   ${NEO_SOURCE_DIR}/opencl/test/unit_test/os_interface/linux/create_drm_memory_manager.cpp
                   ${NEO_SOURCE_DIR}/opencl/test/unit_test/os_interface/linux/drm_neo_create.cpp
                   ${NEO_SOURCE_DIR}/opencl/test/unit_test/os_interface/linux/options.cpp
                   ${NEO_SOURCE_DIR}/opencl/test/unit_test/os_interface/linux/sys_calls_linux_ult.cpp
                   ${NEO_SOURCE_DIR}/shared/test/unit_test/mocks/linux/mock_drm_memory_manager.cpp
                   ${NEO_SOURCE_DIR}/shared/test/unit_test/mocks/linux/mock_drm_memory_manager.h
    )
  else()
    target_sources(${TARGET_NAME} PRIVATE
                   ${NEO_SOURCE_DIR}/opencl/test/unit_test/os_interface/windows/create_wddm_memory_manager.cpp
                   ${NEO_SOURCE_DIR}/opencl/test/unit_test/os_interface/windows/options.cpp
                   ${NEO_SOURCE_DIR}/opencl/test/unit_test/os_interface/windows/sys_calls.cpp
                   ${NEO_SOURCE_DIR}/opencl/test/unit_test/os_interface/windows/ult_dxgi_factory.cpp
                   ${NEO_SOURCE_DIR}/opencl/test/unit_test/os_interface/windows/wddm_calls.cpp
                   ${NEO_SOURCE_DIR}/opencl/test/unit_test/os_interface/windows/wddm_create.cpp
    )
  endif()

  set_property(TARGET ${TARGET_NAME} APPEND_STRING PROPERTY COMPILE_FLAGS ${ASAN_FLAGS})

  target_include_directories(${TARGET_NAME} PRIVATE
                             ${CMAKE_CURRENT_SOURCE_DIR}
                             ${ENGINE_NODE_DIR}
                             ${NEO_SHARED_TEST_DIRECTORY}/unit_test/test_macros/header${BRANCH_DIR_SUFFIX}
  )

  if(UNIX)
    target_link_libraries(${TARGET_NAME} pthread rt)
  else()
    target_link_libraries(${TARGET_NAME} dbghelp)
  endif()

  target_link_libraries(${TARGET_NAME}
                        gmock-gtest
                        ${NEO_STATICALLY_LINKED_LIBRARIES_MOCKABLE}
                        compute_runtime_mockable_extra
  )

  if(MSVC)
    set_target_properties(${TARGET_NAME} PROPERTIES
                          VS_DEBUGGER_COMMAND_ARGUMENTS "--gtest_filter=* --gtest_catch_exceptions=0 --enable_default_listener --disable_pagefaulting_tests"
                          VS_DEBUGGER_WORKING_DIRECTORY "$(OutDir)"
    )
  endif()

  add_subdirectories()
  add_dependencies(${TARGET_NAME} prepare_test_kernel_for_shared)
  create_project_source_tree(${TARGET_NAME})

endif()
