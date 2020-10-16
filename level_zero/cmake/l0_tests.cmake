#
# Copyright (C) 2020 Intel Corporation
#
# SPDX-License-Identifier: MIT
#

##
## L0 tests settings
##

# These need to be added to a project to enable platform support in ULTs
if(TESTS_GEN8)
  set(COMPUTE_RUNTIME_ULT_GEN8
      ${NEO_SHARED_TEST_DIRECTORY}/unit_test/libult/gen8.cpp
      ${NEO_SHARED_TEST_DIRECTORY}/unit_test/gen8/cmd_parse_gen8.cpp
  )
endif()

if(TESTS_GEN9)
  set(COMPUTE_RUNTIME_ULT_GEN9
      ${NEO_SHARED_TEST_DIRECTORY}/unit_test/libult/gen9.cpp
      ${NEO_SHARED_TEST_DIRECTORY}/unit_test/gen9/cmd_parse_gen9.cpp
  )
endif()

if(TESTS_GEN11)
  set(COMPUTE_RUNTIME_ULT_GEN11
      ${NEO_SHARED_TEST_DIRECTORY}/unit_test/libult/gen11.cpp
      ${NEO_SHARED_TEST_DIRECTORY}/unit_test/gen11/cmd_parse_gen11.cpp
  )
endif()

if(TESTS_GEN12LP)
  set(COMPUTE_RUNTIME_ULT_GEN12LP
      ${NEO_SHARED_TEST_DIRECTORY}/unit_test/libult/gen12lp.cpp
      ${NEO_SHARED_TEST_DIRECTORY}/unit_test/gen12lp/cmd_parse_gen12lp.cpp
  )
  include_directories(${NEO_SHARED_TEST_DIRECTORY}/unit_test/gen12lp/cmd_parse${BRANCH_DIR_SUFFIX}/)
endif()

## ULT related settings

#Extract compute runtime COMPILE_DEFINITIONS
get_property(COMPUTE_RUNTIME_MOCKABLE_DEFINITIONS
             TARGET ${NEO_MOCKABLE_LIB_NAME}
             PROPERTY COMPILE_DEFINITIONS
)

if(WIN32)
  set(COMPUTE_RUNTIME_MOCKABLE_DEFINITIONS
      ${COMPUTE_RUNTIME_MOCKABLE_DEFINITIONS}
      WDDM_VERSION_NUMBER=23
      CONST_FROM_WDK_10_0_18328_0=
  )
endif()

#Extract compute runtime INCLUDE_DIRECTORIES
get_property(COMPUTE_RUNTIME_MOCKABLE_INCLUDES
             TARGET ${NEO_MOCKABLE_LIB_NAME}
             PROPERTY INCLUDE_DIRECTORIES
)

# Create a library that has the missing ingredients to link
add_library(compute_runtime_mockable_extra
            STATIC
            EXCLUDE_FROM_ALL
            ${CMAKE_CURRENT_LIST_DIR}/l0_tests.cmake
            ${NEO_SHARED_TEST_DIRECTORY}/unit_test/utilities/cpuintrinsics.cpp
            ${NEO_SHARED_TEST_DIRECTORY}/unit_test/helpers/test_files.cpp
            ${NEO_SHARED_TEST_DIRECTORY}/unit_test/mocks/mock_command_stream_receiver.cpp
            ${NEO_SHARED_TEST_DIRECTORY}/unit_test/mocks/mock_device.cpp
            ${NEO_SOURCE_DIR}/opencl/test/unit_test/aub_stream_mocks/aub_stream_interface_mock.cpp
            ${NEO_SOURCE_DIR}/opencl/test/unit_test/abort.cpp
            ${NEO_SOURCE_DIR}/opencl/test/unit_test/helpers/built_ins_helper.cpp
            ${NEO_SOURCE_DIR}/opencl/test/unit_test/helpers/debug_helpers.cpp
            ${NEO_SOURCE_DIR}/opencl/test/unit_test/libult/os_interface.cpp
            ${NEO_SOURCE_DIR}/opencl/test/unit_test/libult/source_level_debugger_ult.cpp
            ${NEO_SOURCE_DIR}/opencl/test/unit_test/libult/source_level_debugger_library.cpp
            ${NEO_SOURCE_DIR}/opencl/test/unit_test/mocks/mock_cif.cpp
            ${NEO_SOURCE_DIR}/opencl/test/unit_test/mocks/mock_compilers.cpp
            ${NEO_SOURCE_DIR}/opencl/test/unit_test/mocks/mock_gmm_page_table_mngr.cpp
            ${NEO_SOURCE_DIR}/opencl/test/unit_test/libult/create_tbx_sockets.cpp
            ${NEO_SOURCE_DIR}/opencl/test/unit_test/mocks/mock_deferred_deleter.cpp
            ${NEO_SOURCE_DIR}/opencl/test/unit_test/mocks${BRANCH_SUFIX_DIR}/mock_gmm_client_context.cpp
            ${NEO_SOURCE_DIR}/opencl/test/unit_test/mocks/mock_gmm_client_context_base.cpp
            ${NEO_SOURCE_DIR}/opencl/test/unit_test/mocks/mock_gmm_resource_info.cpp
            ${NEO_SOURCE_DIR}/opencl/test/unit_test/mocks/mock_memory_manager.cpp
            ${NEO_SOURCE_DIR}/opencl/test/unit_test/mocks/mock_program.cpp
            ${NEO_SOURCE_DIR}/opencl/test/unit_test/mocks/mock_sip.cpp
            ${NEO_SOURCE_DIR}/opencl/test/unit_test/utilities/debug_settings_reader_creator.cpp
            ${NEO_SOURCE_DIR}/shared/source/debug_settings/debug_settings_manager.cpp
)

set_property(TARGET compute_runtime_mockable_extra APPEND_STRING PROPERTY COMPILE_FLAGS ${ASAN_FLAGS} ${TSAN_FLAGS})

# These need to be added to a project to enable platform support in ULTs
#Additional includes for ULT builds
target_include_directories(compute_runtime_mockable_extra
                           PUBLIC
                           ${COMPUTE_RUNTIME_MOCKABLE_INCLUDES}
                           ${NEO_SOURCE_DIR}/opencl/test/unit_test/mocks/gmm_memory
                           ${SOURCE_LEVEL_DEBUGGER_HEADERS_DIR}
)

#Additional compile definitions for ULT builds
target_compile_definitions(compute_runtime_mockable_extra
                           PUBLIC
                           ${COMPUTE_RUNTIME_MOCKABLE_DEFINITIONS}
)

target_link_libraries(compute_runtime_mockable_extra
                      gmock-gtest
)

if(WIN32)
  target_sources(compute_runtime_mockable_extra
                 PRIVATE
                 ${NEO_SOURCE_DIR}/shared/source/dll/windows/environment_variables.cpp
                 ${NEO_SOURCE_DIR}/opencl/test/unit_test/mocks/mock_gmm_memory_base.cpp
                 ${NEO_SOURCE_DIR}/opencl/test/unit_test/mocks/mock_wddm.cpp
  )

  target_link_libraries(compute_runtime_mockable_extra
                        ws2_32
  )
endif()

if(UNIX)
  target_sources(compute_runtime_mockable_extra
                 PRIVATE
                 ${NEO_SOURCE_DIR}/opencl/source/dll/linux/allocator_helper.cpp
                 ${NEO_SOURCE_DIR}/opencl/test/unit_test/os_interface/linux/drm_mock.cpp
                 ${NEO_SOURCE_DIR}/shared/source/tbx/tbx_sockets_imp.cpp
  )
  target_link_libraries(compute_runtime_mockable_extra
                        dl
  )
endif()
set_target_properties(compute_runtime_mockable_extra PROPERTIES POSITION_INDEPENDENT_CODE ON)
