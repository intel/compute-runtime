#
# Copyright (C) 2017-2018 Intel Corporation
#
# SPDX-License-Identifier: MIT
#

project(igdrcl_aub_tests)

include(${CMAKE_CURRENT_SOURCE_DIR}/common.cmake)

list(APPEND IGDRCL_AUB_TESTS__TARGET_OBJECTS
  ${CMAKE_CURRENT_SOURCE_DIR}/aub_tests_configuration.cpp
  ${IGDRCL_SOURCE_DIR}/runtime/aub/aub_stream_interface.cpp
  $<TARGET_OBJECTS:${BUILTINS_SOURCES_LIB_NAME}>
  $<TARGET_OBJECTS:${SCHEDULER_BINARY_LIB_NAME}>
  $<TARGET_OBJECTS:igdrcl_libult>
  $<TARGET_OBJECTS:igdrcl_libult_cs>
  $<TARGET_OBJECTS:igdrcl_libult_env>
)

if(DEFINED AUB_STREAM_DIR)
  list(APPEND IGDRCL_AUB_TESTS__TARGET_OBJECTS $<TARGET_OBJECTS:${AUB_STREAM_ENABLE_LIB_NAME}>)
endif()

add_executable(igdrcl_aub_tests
  ${IGDRCL_AUB_TESTS__TARGET_OBJECTS}
)

set_target_properties(igdrcl_aub_tests PROPERTIES FOLDER ${TEST_PROJECTS_FOLDER})

if(WIN32)
  set_target_properties(igdrcl_aub_tests PROPERTIES
    VS_DEBUGGER_WORKING_DIRECTORY ${TargetDir}
  )
endif()

add_custom_target(run_aub_tests)
set_target_properties(run_aub_tests PROPERTIES FOLDER ${TEST_PROJECTS_FOLDER})

if(WIN32)
  target_include_directories(igdrcl_aub_tests PRIVATE
    ${IGDRCL_SOURCE_DIR}/unit_tests/mocks${BRANCH_DIR_SUFFIX}
  )
endif()

target_sources(igdrcl_aub_tests PRIVATE
  ${IGDRCL_SOURCE_DIR}/runtime/gmm_helper/resource_info.cpp
)

if(WIN32)
  target_sources(igdrcl_aub_tests PRIVATE
    ${IGDRCL_SOURCE_DIR}/runtime/gmm_helper/gmm_memory.cpp
    ${IGDRCL_SOURCE_DIR}/unit_tests/os_interface/windows/wddm_create.cpp
  )
endif()

copy_gmm_dll_for(igdrcl_aub_tests)
add_subdirectories()

target_link_libraries(igdrcl_aub_tests igdrcl_mocks)
target_link_libraries(igdrcl_aub_tests ${NEO_MOCKABLE_LIB_NAME})
target_link_libraries(igdrcl_aub_tests igdrcl_mocks)
target_link_libraries(igdrcl_aub_tests gmock-gtest ${IGDRCL_EXTRA_LIBS})

target_include_directories(igdrcl_aub_tests BEFORE PRIVATE ${IGDRCL_SOURCE_DIR}/unit_tests/gen_common${BRANCH_DIR_SUFFIX})

macro(macro_for_each_test_config)
  neo_run_aub_target(${PLATFORM_IT_LOWER} "${PLATFORM_IT} ${SLICES}x${SUBSLICES}x${EU_PER_SS}" ${PLATFORM_IT_LOWER} ${SLICES} ${SUBSLICES} ${EU_PER_SS})
endmacro()

macro(macro_for_each_platform)
  add_custom_target(${PLATFORM_IT_LOWER}_aub_tests
    DEPENDS igdrcl_aub_tests
    DEPENDS copy_test_files_${PLATFORM_IT_LOWER}
  )
  foreach(PLATFORM_TYPE "CORE" "LP")
    if(${PLATFORM_IT}_IS_${PLATFORM_TYPE})
      get_family_name_with_type(${GEN_TYPE} ${PLATFORM_TYPE})
      add_dependencies(${PLATFORM_IT_LOWER}_aub_tests test_kernels_${family_name_with_type})
      add_dependencies(${PLATFORM_IT_LOWER}_aub_tests test_kernel_${family_name_with_type})
      PLATFORM_HAS_2_0(${GEN_TYPE} ${PLATFORM_IT} PLATFORM_SUPPORTS_2_0)
      if(PLATFORM_SUPPORTS_2_0)
        add_dependencies(${PLATFORM_IT_LOWER}_aub_tests test_kernel_2_0_${family_name_with_type})
      endif()
    endif()
  endforeach()
  add_custom_target(run_${PLATFORM_IT_LOWER}_aub_tests ALL DEPENDS ${PLATFORM_IT_LOWER}_aub_tests)
  add_dependencies(run_aub_tests run_${PLATFORM_IT_LOWER}_aub_tests)
  set_target_properties(${PLATFORM_IT_LOWER}_aub_tests PROPERTIES FOLDER "${PLATFORM_SPECIFIC_TARGETS_FOLDER}/${PLATFORM_IT_LOWER}")
  set_target_properties(run_${PLATFORM_IT_LOWER}_aub_tests PROPERTIES FOLDER "${PLATFORM_SPECIFIC_TARGETS_FOLDER}/${PLATFORM_IT_LOWER}")
  add_custom_command(
    TARGET ${PLATFORM_IT_LOWER}_aub_tests
    POST_BUILD
    COMMAND WORKING_DIRECTORY ${TargetDir}
    COMMAND echo Creating output directory for ${PLATFORM_IT_LOWER} AUBs generation...
    COMMAND ${CMAKE_COMMAND} -E make_directory "${TargetDir}/${PLATFORM_IT_LOWER}_aub/aub_out"
  )
  if(WIN32)
    add_dependencies(${PLATFORM_IT_LOWER}_aub_tests mock_gdi)
  endif()
  apply_macro_for_each_test_config("AUB_TESTS")
endmacro()

macro(macro_for_each_gen)
  apply_macro_for_each_platform()
endmacro()

apply_macro_for_each_gen("TESTED")
create_project_source_tree(igdrcl_aub_tests ${IGDRCL_SOURCE_DIR}/runtime ${IGDRCL_SOURCE_DIR}/unit_tests)
