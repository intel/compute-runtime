#
# Copyright (C) 2025 Intel Corporation
#
# SPDX-License-Identifier: MIT
#

if(NOT NEO_SKIP_MT_UNIT_TESTS)
  link_libraries(${ASAN_LIBS} ${TSAN_LIBS})

  set(TARGET_NAME ${TARGET_NAME_L0}_core_mt_tests)

  include(${NEO_SOURCE_DIR}/cmake/setup_ult_global_flags.cmake)

  function(ADD_SUPPORTED_TEST_PRODUCT_FAMILIES_DEFINITION)
    set(L0_TESTED_PRODUCT_FAMILIES ${ALL_TESTED_PRODUCT_FAMILY})
    string(REPLACE ";" "," L0_TESTED_PRODUCT_FAMILIES "${L0_TESTED_PRODUCT_FAMILIES}")
    add_definitions(-DSUPPORTED_TEST_PRODUCT_FAMILIES=${L0_TESTED_PRODUCT_FAMILIES})
  endfunction()

  ADD_SUPPORTED_TEST_PRODUCT_FAMILIES_DEFINITION()

  add_executable(${TARGET_NAME} EXCLUDE_FROM_ALL
                 ${NEO_SOURCE_DIR}/level_zero/core/source/dll/disallow_deferred_deleter.cpp

  )

  target_sources(${TARGET_NAME} PRIVATE
                 ${CMAKE_CURRENT_SOURCE_DIR}/CMakeLists.txt
                 ${NEO_SOURCE_DIR}/level_zero/core/test/unit_tests/mock.h
                 ${NEO_SOURCE_DIR}/level_zero/core/test/unit_tests/white_box.h
                 ${NEO_SOURCE_DIR}/level_zero/core/test/unit_tests/sources/builtin/create_ult_builtin_functions_lib.cpp
                 ${NEO_SOURCE_DIR}/level_zero/tools/test/unit_tests/sources/debug/debug_session_helper.cpp
                 ${NEO_SHARED_TEST_DIRECTORY}/common/common_main.cpp
                 ${NEO_SHARED_TEST_DIRECTORY}/common/helpers/mock_sip_listener.cpp
                 ${NEO_SHARED_TEST_DIRECTORY}/common/helpers/virtual_file_system_listener.cpp
                 ${NEO_SHARED_TEST_DIRECTORY}/common/tests_configuration.h
                 ${NEO_SOURCE_DIR}/level_zero/core/test/common/test_modules/gen_kernel.cmake
                 ${NEO_SOURCE_DIR}/level_zero/core/test/common/ult_specific_config_l0.cpp
                 ${NEO_SOURCE_DIR}/level_zero/core/test/common/ult_config_listener_l0.cpp
                 ${NEO_SOURCE_DIR}/level_zero/core/test/common/ult_config_listener_l0.h
  )

  target_sources(${TARGET_NAME} PRIVATE
                 $<TARGET_OBJECTS:${L0_MOCKABLE_LIB_NAME}>
                 $<TARGET_OBJECTS:neo_libult_common>
                 $<TARGET_OBJECTS:neo_libult_cs>
                 $<TARGET_OBJECTS:neo_libult>
                 $<TARGET_OBJECTS:neo_shared_mocks>
                 $<TARGET_OBJECTS:neo_mt_tests_config>
  )

  set_target_properties(${TARGET_NAME} PROPERTIES FOLDER ${TARGET_NAME_L0})
  set_property(TARGET ${TARGET_NAME} PROPERTY ENABLE_EXPORTS TRUE)

  add_subdirectoriesL0(${CMAKE_CURRENT_SOURCE_DIR} "*")

  target_compile_definitions(${TARGET_NAME} PRIVATE $<TARGET_PROPERTY:${L0_MOCKABLE_LIB_NAME},INTERFACE_COMPILE_DEFINITIONS>)
  target_include_directories(${TARGET_NAME} PRIVATE $<TARGET_PROPERTY:${L0_MOCKABLE_LIB_NAME},INTERFACE_INCLUDE_DIRECTORIES>)

  target_include_directories(${TARGET_NAME}
                             BEFORE
                             PRIVATE
                             ${NEO_SHARED_TEST_DIRECTORY}/common/test_macros/header${BRANCH_DIR_SUFFIX}
                             ${NEO_SHARED_TEST_DIRECTORY}/common/test_configuration/mt_tests
  )

  target_link_libraries(${TARGET_NAME}
                        ${NEO_SHARED_MOCKABLE_LIB_NAME}
                        ${HW_LIBS_ULT}
                        gmock-gtest
                        ${NEO_EXTRA_LIBS}
  )

  target_sources(${TARGET_NAME} PRIVATE
                 $<TARGET_OBJECTS:mock_aubstream>
                 $<TARGET_OBJECTS:mock_gmm>
                 $<TARGET_OBJECTS:${TARGET_NAME_L0}_fixtures>
                 $<TARGET_OBJECTS:${TARGET_NAME_L0}_mocks>
                 $<TARGET_OBJECTS:${BUILTINS_BINARIES_STATELESS_LIB_NAME}>
                 $<TARGET_OBJECTS:${BUILTINS_BINARIES_HEAPLESS_LIB_NAME}>
                 $<TARGET_OBJECTS:${BUILTINS_BINARIES_BINDFUL_LIB_NAME}>
                 $<TARGET_OBJECTS:${BUILTINS_BINARIES_BINDLESS_LIB_NAME}>
  )
  if(TARGET ${BUILTINS_SPIRV_LIB_NAME})
    target_sources(${TARGET_NAME} PRIVATE
                   $<TARGET_OBJECTS:${BUILTINS_SPIRV_LIB_NAME}>
    )
  endif()

  option(L0_ULT_VERBOSE "Use the default/verbose test output" OFF)
  if(NOT L0_ULT_VERBOSE)
    set(L0_TESTS_LISTENER_OPTION "--disable_default_listener")
  else()
    set(L0_TESTS_LISTENER_OPTION "--enable_default_listener")
  endif()

  create_source_tree(${TARGET_NAME} ${L0_ROOT_DIR}/..)
endif()
