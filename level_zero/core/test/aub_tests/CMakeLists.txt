#
# Copyright (C) 2021-2025 Intel Corporation
#
# SPDX-License-Identifier: MIT
#

if(NOT NEO_SKIP_AUB_TESTS AND DEFINED AUB_STREAM_PROJECT_NAME)
  link_libraries(${ASAN_LIBS} ${TSAN_LIBS})

  set(TARGET_NAME ${TARGET_NAME_L0}_aub_tests)
  set(L0_AUB_TEST_DIR ${CMAKE_CURRENT_SOURCE_DIR})

  list(APPEND L0_AUB_TESTS__TARGET_OBJECTS
       ${CMAKE_CURRENT_SOURCE_DIR}/aub_hello_world_test.cpp
       ${NEO_SHARED_TEST_DIRECTORY}/common/common_main.cpp
       ${NEO_SHARED_TEST_DIRECTORY}/common/helpers/virtual_file_system_listener.cpp
       ${NEO_SHARED_TEST_DIRECTORY}/common/helpers/mock_sip_listener.cpp
       ${NEO_SHARED_TEST_DIRECTORY}/common/tests_configuration.h
  )

  function(ADD_SUPPORTED_TEST_PRODUCT_FAMILIES_DEFINITION)
    set(L0_TESTED_PRODUCT_FAMILIES ${ALL_TESTED_PRODUCT_FAMILY})
    string(REPLACE ";" "," L0_TESTED_PRODUCT_FAMILIES "${L0_TESTED_PRODUCT_FAMILIES}")
    add_definitions(-DSUPPORTED_TEST_PRODUCT_FAMILIES=${L0_TESTED_PRODUCT_FAMILIES})
  endfunction()

  ADD_SUPPORTED_TEST_PRODUCT_FAMILIES_DEFINITION()

  list(APPEND L0_AUB_TESTS__TARGET_OBJECTS $<TARGET_OBJECTS:${AUB_STREAM_PROJECT_NAME}_all_hw>)

  add_executable(${TARGET_NAME}
                 ${L0_AUB_TESTS__TARGET_OBJECTS}
  )

  set_target_properties(${TARGET_NAME} PROPERTIES FOLDER ${TARGET_NAME_L0})

  if(MSVC)
    set_target_properties(${TARGET_NAME} PROPERTIES VS_DEBUGGER_WORKING_DIRECTORY ${TargetDir})
  endif()

  set_property(TARGET ${TARGET_NAME} PROPERTY ENABLE_EXPORTS TRUE)

  target_sources(${TARGET_NAME} PRIVATE
                 ${CMAKE_CURRENT_SOURCE_DIR}/CMakeLists.txt
                 ${L0_CORE_ENABLERS}
                 ${NEO_SOURCE_DIR}/level_zero/core/test/common/ult_specific_config_l0.cpp
                 ${NEO_SOURCE_DIR}/level_zero/core/test/common/ult_config_listener_l0.cpp
                 ${NEO_SOURCE_DIR}/level_zero/core/test/common/ult_config_listener_l0.h
                 ${NEO_SOURCE_DIR}/level_zero/core/test/unit_tests/mock.h
                 ${NEO_SOURCE_DIR}/level_zero/core/test/unit_tests/white_box.h
                 ${NEO_SOURCE_DIR}/level_zero/tools/test/unit_tests/sources/debug/debug_session_helper.cpp
                 ${NEO_SOURCE_DIR}/level_zero/core/source/dll/disallow_deferred_deleter.cpp
  )

  target_sources(${TARGET_NAME} PRIVATE
                 $<TARGET_OBJECTS:${L0_MOCKABLE_LIB_NAME}>
                 $<TARGET_OBJECTS:${TARGET_NAME_L0}_fixtures>
                 $<TARGET_OBJECTS:${TARGET_NAME_L0}_mocks>
                 $<TARGET_OBJECTS:neo_libult_common>
                 $<TARGET_OBJECTS:neo_libult_cs>
                 $<TARGET_OBJECTS:neo_libult>
                 $<TARGET_OBJECTS:neo_shared_mocks>
                 $<TARGET_OBJECTS:${BUILTINS_BINARIES_STATELESS_LIB_NAME}>
                 $<TARGET_OBJECTS:${BUILTINS_BINARIES_HEAPLESS_LIB_NAME}>
                 $<TARGET_OBJECTS:${BUILTINS_BINARIES_BINDFUL_LIB_NAME}>
                 $<TARGET_OBJECTS:${BUILTINS_BINARIES_BINDLESS_LIB_NAME}>
                 $<TARGET_OBJECTS:neo_aub_tests_config>
  )
  if(TARGET ${BUILTINS_SPIRV_LIB_NAME})
    target_sources(${TARGET_NAME} PRIVATE
                   $<TARGET_OBJECTS:${BUILTINS_SPIRV_LIB_NAME}>
    )
  endif()

  copy_gmm_dll_for(${TARGET_NAME})
  add_subdirectories()

  target_link_libraries(${TARGET_NAME} ${NEO_SHARED_MOCKABLE_LIB_NAME})
  target_link_libraries(${TARGET_NAME} ${HW_LIBS_ULT})
  target_link_libraries(${TARGET_NAME} gmock-gtest)
  target_link_libraries(${TARGET_NAME} ${NEO_EXTRA_LIBS})
  target_link_libraries(${TARGET_NAME} ${AUB_STREAM_PROJECT_NAME})

  if(UNIX)
    target_link_libraries(${TARGET_NAME} rt)
    target_link_libraries(${TARGET_NAME} ${GMM_LINK_NAME})
  else()
    target_link_libraries(${TARGET_NAME} dbghelp)
    add_dependencies(${TARGET_NAME} ${GMM_TARGET_NAME})
  endif()

  target_include_directories(${TARGET_NAME} PRIVATE
                             ${NEO_SHARED_TEST_DIRECTORY}/common/test_configuration/aub_tests
                             ${NEO_SOURCE_DIR}/shared/test/common/helpers/includes${BRANCH_DIR_SUFFIX}
                             ${NEO_SHARED_TEST_DIRECTORY}/common/test_macros/header${BRANCH_DIR_SUFFIX}
                             ${ENGINE_NODE_DIR}
                             ${CIF_BASE_DIR}
                             ${NEO__GMM_INCLUDE_DIR}
                             ${WDK_DIR}
                             ${NEO_SOURCE_DIR}/level_zero/include
  )

  create_project_source_tree(${TARGET_NAME})
endif()
