#
# Copyright (C) 2020-2025 Intel Corporation
#
# SPDX-License-Identifier: MIT
#

string(REPLACE "/" ";" unit_test_config ${unit_test_config})
list(GET unit_test_config 0 product)
list(GET unit_test_config 1 slices)
list(GET unit_test_config 2 subslices)
list(GET unit_test_config 3 eu_per_ss)
list(GET unit_test_config 4 revision_id)
list(LENGTH unit_test_config unit_test_config_num_entries)
if(${unit_test_config_num_entries} GREATER_EQUAL 6)
  list(GET unit_test_config 5 device_id)
else()
  set(device_id 0)
endif()

if(${unit_test_config_num_entries} GREATER_EQUAL 7)
  list(GET unit_test_config 6 test_mask)
else()
  set(test_mask 0)
endif()

math(EXPR skip_shared "${test_mask} & 1" OUTPUT_FORMAT DECIMAL)
math(EXPR skip_ocl "${test_mask} & 2" OUTPUT_FORMAT DECIMAL)
math(EXPR skip_l0 "${test_mask} & 4" OUTPUT_FORMAT DECIMAL)

if(NOT NEO_SKIP_SHARED_UNIT_TESTS)
  if(NOT skip_shared)
    unset(GTEST_OUTPUT)
    if(DEFINED GTEST_OUTPUT_DIR)
      set(GTEST_OUTPUT "--gtest_output=json:${GTEST_OUTPUT_DIR}/shared-${product}-${revision_id}-unit_tests-results.json")
    endif()

    if(NOT TARGET run_shared_tests)
      add_custom_target(run_shared_tests)
    endif()
    set_target_properties(run_shared_tests PROPERTIES FOLDER ${PLATFORM_SPECIFIC_TEST_TARGETS_FOLDER})
    add_dependencies(run_unit_tests run_shared_tests)

    add_custom_target(run_${product}_${revision_id}_shared_tests DEPENDS unit_tests)
    set_target_properties(run_${product}_${revision_id}_shared_tests PROPERTIES FOLDER "${PLATFORM_SPECIFIC_TEST_TARGETS_FOLDER}/${product}/${revision_id}")

    add_custom_command(
                       TARGET run_${product}_${revision_id}_shared_tests
                       POST_BUILD
                       COMMAND WORKING_DIRECTORY ${TargetDir}
                       COMMAND echo Running neo_shared_tests ${target} ${slices}x${subslices}x${eu_per_ss} in ${TargetDir}
                       COMMAND echo Cmd line: $<TARGET_FILE:neo_shared_tests> --product ${product} --slices ${slices} --subslices ${subslices} --eu_per_ss ${eu_per_ss} ${GTEST_EXCEPTION_OPTIONS} --gtest_repeat=${GTEST_REPEAT} ${GTEST_SHUFFLE} ${GTEST_OUTPUT} ${NEO_TESTS_LISTENER_OPTION} ${GTEST_FILTER_OPTION} --rev_id ${revision_id} --dev_id ${device_id}
                       COMMAND ${CMAKE_COMMAND} -E make_directory ${TargetDir}/shared/${product}/${revision_id}
                       COMMAND ${NEO_RUN_INTERCEPTOR_LIST} $<TARGET_FILE:neo_shared_tests> --product ${product} --slices ${slices} --subslices ${subslices} --eu_per_ss ${eu_per_ss} ${GTEST_EXCEPTION_OPTIONS} --gtest_repeat=${GTEST_REPEAT} ${GTEST_SHUFFLE} ${GTEST_OUTPUT} ${NEO_TESTS_LISTENER_OPTION} ${GTEST_FILTER_OPTION} --rev_id ${revision_id} --dev_id ${device_id}
    )

    add_dependencies(run_shared_tests run_${product}_${revision_id}_shared_tests)
  endif()
endif()

if(NOT NEO_SKIP_OCL_UNIT_TESTS)
  if(NOT skip_ocl)
    unset(GTEST_OUTPUT)
    if(DEFINED GTEST_OUTPUT_DIR)
      set(GTEST_OUTPUT "--gtest_output=json:${GTEST_OUTPUT_DIR}/ocl-${product}-${revision_id}-unit_tests-results.json")
    endif()

    if(NOT TARGET run_ocl_tests)
      add_custom_target(run_ocl_tests)
    endif()
    set_target_properties(run_ocl_tests PROPERTIES FOLDER ${PLATFORM_SPECIFIC_TEST_TARGETS_FOLDER})
    add_dependencies(run_unit_tests run_ocl_tests)

    add_custom_target(run_${product}_${revision_id}_ocl_tests DEPENDS unit_tests)
    set_target_properties(run_${product}_${revision_id}_ocl_tests PROPERTIES FOLDER "${PLATFORM_SPECIFIC_TEST_TARGETS_FOLDER}/${product}/${revision_id}")

    set(outputdir "${TargetDir}/opencl/${product}/${revision_id}")
    add_custom_command(
                       TARGET run_${product}_${revision_id}_ocl_tests
                       POST_BUILD
                       COMMAND WORKING_DIRECTORY ${TargetDir}
                       COMMAND echo Running igdrcl_tests ${target} ${slices}x${subslices}x${eu_per_ss} in ${TargetDir}
                       COMMAND echo Cmd line: ${GTEST_ENV} ${NEO_RUN_INTERCEPTOR_LIST} $<TARGET_FILE:igdrcl_tests> --product ${product} --slices ${slices} --subslices ${subslices} --eu_per_ss ${eu_per_ss} ${GTEST_EXCEPTION_OPTIONS} --gtest_repeat=${GTEST_REPEAT} ${GTEST_SHUFFLE} ${GTEST_OUTPUT} ${NEO_TESTS_LISTENER_OPTION} ${GTEST_FILTER_OPTION} --rev_id ${revision_id} --dev_id ${device_id}
                       COMMAND ${GTEST_ENV} ${NEO_RUN_INTERCEPTOR_LIST} $<TARGET_FILE:igdrcl_tests> --product ${product} --slices ${slices} --subslices ${subslices} --eu_per_ss ${eu_per_ss} ${GTEST_EXCEPTION_OPTIONS} --gtest_repeat=${GTEST_REPEAT} ${GTEST_SHUFFLE} ${GTEST_OUTPUT} ${NEO_TESTS_LISTENER_OPTION} ${GTEST_FILTER_OPTION} --rev_id ${revision_id} --dev_id ${device_id}
    )

    if(WIN32 AND ${CMAKE_BUILD_TYPE} STREQUAL "Debug" AND "${IGDRCL_OPTION__BITS}" STREQUAL "64" AND APPVERIFIER_ALLOWED)
      unset(GTEST_OUTPUT)
      if(DEFINED GTEST_OUTPUT_DIR)
        set(GTEST_OUTPUT "--gtest_output=json:${GTEST_OUTPUT_DIR}/ocl_windows-${product}-${revision_id}-unit_tests-results.json")
      endif()
      add_custom_command(
                         TARGET run_${product}_${revision_id}_ocl_tests
                         POST_BUILD
                         COMMAND WORKING_DIRECTORY ${TargetDir}
                         COMMAND echo Running igdrcl_tests with App Verifier
                         COMMAND ${NEO_SOURCE_DIR}/scripts/verify.bat $<TARGET_FILE:igdrcl_tests> --product ${product} --slices ${slices} --subslices ${subslices} --eu_per_ss ${eu_per_ss} ${GTEST_EXCEPTION_OPTIONS} ${GTEST_OUTPUT} ${NEO_TESTS_LISTENER_OPTION} ${GTEST_FILTER_OPTION} --rev_id ${revision_id} --dev_id ${device_id}
                         COMMAND echo App Verifier returned: %errorLevel%
      )
    endif()

    add_dependencies(run_ocl_tests run_${product}_${revision_id}_ocl_tests)
  endif()
endif()

if(NOT NEO_SKIP_L0_UNIT_TESTS AND BUILD_WITH_L0)
  if(NOT skip_l0)
    unset(GTEST_OUTPUT_CORE)
    if(DEFINED GTEST_OUTPUT_DIR)
      set(GTEST_OUTPUT_CORE "--gtest_output=json:${GTEST_OUTPUT_DIR}/ze_intel_gpu_core-${product}-${revision_id}-unit_tests-results.json")
      message(STATUS "GTest output core set to ${GTEST_OUTPUT_CORE}")
    endif()
    unset(GTEST_OUTPUT_TOOLS)
    if(DEFINED GTEST_OUTPUT_DIR)
      set(GTEST_OUTPUT_TOOLS "--gtest_output=json:${GTEST_OUTPUT_DIR}/ze_intel_gpu_tools-${product}-${revision_id}-unit_tests-results.json")
      message(STATUS "GTest output tools set to ${GTEST_OUTPUT_TOOLS}")
    endif()
    unset(GTEST_OUTPUT_EXP)
    if(DEFINED GTEST_OUTPUT_DIR)
      set(GTEST_OUTPUT_EXP "--gtest_output=json:${GTEST_OUTPUT_DIR}/ze_intel_gpu_exp-${product}-${revision_id}-unit_tests-results.json")
      message(STATUS "GTest output exp set to ${GTEST_OUTPUT_EXP}")
    endif()
    unset(GTEST_OUTPUT_SYSMAN)
    if(DEFINED GTEST_OUTPUT_DIR)
      set(GTEST_OUTPUT_SYSMAN "--gtest_output=json:${GTEST_OUTPUT_DIR}/ze_intel_gpu_sysman-${product}-${revision_id}-unit_tests-results.json")
      message(STATUS "GTest output tools set to ${GTEST_OUTPUT_SYSMAN}")
    endif()

    if(NOT TARGET run_l0_tests)
      add_custom_target(run_l0_tests)
    endif()
    set_target_properties(run_l0_tests PROPERTIES FOLDER ${PLATFORM_SPECIFIC_TEST_TARGETS_FOLDER})
    add_dependencies(run_unit_tests run_l0_tests)

    add_custom_target(run_${product}_${revision_id}_l0_tests DEPENDS unit_tests)
    set_target_properties(run_${product}_${revision_id}_l0_tests PROPERTIES FOLDER "${PLATFORM_SPECIFIC_TEST_TARGETS_FOLDER}/${product}/${revision_id}")

    add_custom_command(
                       TARGET run_${product}_${revision_id}_l0_tests
                       POST_BUILD
                       COMMAND WORKING_DIRECTORY ${TargetDir}
                       COMMAND echo create working directory ${TargetDir}/level_zero/${product}/${revision_id}
                       COMMAND ${CMAKE_COMMAND} -E make_directory ${TargetDir}/level_zero/${product}/${revision_id}
                       COMMAND echo Running ze_intel_gpu_core_tests ${target} ${slices}x${subslices}x${eu_per_ss} in ${TargetDir}
                       COMMAND echo Cmd line: ${NEO_RUN_INTERCEPTOR_LIST} $<TARGET_FILE:ze_intel_gpu_core_tests> --product ${product} --slices ${slices} --subslices ${subslices} --eu_per_ss ${eu_per_ss} ${GTEST_EXCEPTION_OPTIONS} --gtest_repeat=${GTEST_REPEAT} ${GTEST_SHUFFLE} ${GTEST_OUTPUT_CORE} ${NEO_TESTS_LISTENER_OPTION} ${GTEST_FILTER_OPTION} --rev_id ${revision_id} --dev_id ${device_id}
                       COMMAND ${NEO_RUN_INTERCEPTOR_LIST} $<TARGET_FILE:ze_intel_gpu_core_tests> --product ${product} --slices ${slices} --subslices ${subslices} --eu_per_ss ${eu_per_ss} ${GTEST_EXCEPTION_OPTIONS} --gtest_repeat=${GTEST_REPEAT} ${GTEST_SHUFFLE} ${GTEST_OUTPUT_CORE} ${NEO_TESTS_LISTENER_OPTION} ${GTEST_FILTER_OPTION} --rev_id ${revision_id} --dev_id ${device_id}
                       COMMAND echo Running ze_intel_gpu_tools_tests ${target} ${slices}x${subslices}x${eu_per_ss} in ${TargetDir}
                       COMMAND echo Cmd line: ${NEO_RUN_INTERCEPTOR_LIST} $<TARGET_FILE:ze_intel_gpu_tools_tests> --product ${product} --slices ${slices} --subslices ${subslices} --eu_per_ss ${eu_per_ss} ${GTEST_EXCEPTION_OPTIONS} --gtest_repeat=${GTEST_REPEAT} ${GTEST_SHUFFLE} ${GTEST_OUTPUT_CORE} ${NEO_TESTS_LISTENER_OPTION} ${GTEST_FILTER_OPTION} --rev_id ${revision_id} --dev_id ${device_id}
                       COMMAND ${NEO_RUN_INTERCEPTOR_LIST} $<TARGET_FILE:ze_intel_gpu_tools_tests> --product ${product} --slices ${slices} --subslices ${subslices} --eu_per_ss ${eu_per_ss} ${GTEST_EXCEPTION_OPTIONS} --gtest_repeat=${GTEST_REPEAT} ${GTEST_SHUFFLE} ${GTEST_OUTPUT_TOOLS} ${NEO_TESTS_LISTENER_OPTION} ${GTEST_FILTER_OPTION} --rev_id ${revision_id} --dev_id ${device_id}
                       COMMAND echo Running ze_intel_gpu_sysman_tests ${target} ${slices}x${subslices}x${eu_per_ss} in ${TargetDir}
                       COMMAND echo Cmd line: ${NEO_RUN_INTERCEPTOR_LIST} $<TARGET_FILE:ze_intel_gpu_sysman_tests> --product ${product} --slices ${slices} --subslices ${subslices} --eu_per_ss ${eu_per_ss} ${GTEST_EXCEPTION_OPTIONS} --gtest_repeat=${GTEST_REPEAT} ${GTEST_SHUFFLE} ${GTEST_OUTPUT_CORE} ${NEO_TESTS_LISTENER_OPTION} ${GTEST_FILTER_OPTION} --rev_id ${revision_id} --dev_id ${device_id}
                       COMMAND ${NEO_RUN_INTERCEPTOR_LIST} $<TARGET_FILE:ze_intel_gpu_sysman_tests> --product ${product} --slices ${slices} --subslices ${subslices} --eu_per_ss ${eu_per_ss} ${GTEST_EXCEPTION_OPTIONS} --gtest_repeat=${GTEST_REPEAT} ${GTEST_SHUFFLE} ${GTEST_OUTPUT_SYSMAN} ${NEO_TESTS_LISTENER_OPTION} ${GTEST_FILTER_OPTION} --rev_id ${revision_id} --dev_id ${device_id}
    )

    add_dependencies(run_l0_tests run_${product}_${revision_id}_l0_tests)

    if(NOT NEO_SKIP_MT_UNIT_TESTS)
      if(NOT TARGET run_l0_mt_tests)
        add_custom_target(run_l0_mt_tests)
      endif()
      set_target_properties(run_l0_mt_tests PROPERTIES FOLDER ${PLATFORM_SPECIFIC_TEST_TARGETS_FOLDER})
      add_dependencies(run_mt_unit_tests run_l0_mt_tests)

      add_custom_target(run_${product}_${revision_id}_l0_mt_tests)
      set_target_properties(run_${product}_${revision_id}_l0_mt_tests PROPERTIES FOLDER "${PLATFORM_SPECIFIC_TEST_TARGETS_FOLDER}/${product}/${revision_id}")

      add_custom_command(
                         TARGET run_${product}_${revision_id}_l0_mt_tests
                         POST_BUILD
                         COMMAND WORKING_DIRECTORY ${TargetDir}
                         COMMAND echo create working directory ${TargetDir}/level_zero/${product}/${revision_id}
                         COMMAND ${CMAKE_COMMAND} -E make_directory ${TargetDir}/level_zero/${product}/${revision_id}
                         COMMAND echo Running ze_intel_gpu_core_mt_tests ${target} ${slices}x${subslices}x${eu_per_ss} in ${TargetDir}
                         COMMAND echo Cmd line: ${NEO_RUN_INTERCEPTOR_LIST} $<TARGET_FILE:ze_intel_gpu_core_mt_tests> --product ${product} --slices ${slices} --subslices ${subslices} --eu_per_ss ${eu_per_ss} ${GTEST_EXCEPTION_OPTIONS} --gtest_repeat=${GTEST_REPEAT} ${GTEST_SHUFFLE} ${GTEST_OUTPUT_L0MT} ${NEO_TESTS_LISTENER_OPTION} ${GTEST_FILTER_OPTION} --rev_id ${revision_id} --dev_id ${device_id}
                         COMMAND ${NEO_RUN_INTERCEPTOR_LIST} $<TARGET_FILE:ze_intel_gpu_core_mt_tests> --product ${product} --slices ${slices} --subslices ${subslices} --eu_per_ss ${eu_per_ss} ${GTEST_EXCEPTION_OPTIONS} --gtest_repeat=${GTEST_REPEAT} ${GTEST_SHUFFLE} ${GTEST_OUTPUT_L0MT} ${NEO_TESTS_LISTENER_OPTION} ${GTEST_FILTER_OPTION} --rev_id ${revision_id} --dev_id ${device_id}
      )

      add_dependencies(run_l0_mt_tests run_${product}_${revision_id}_l0_mt_tests)
    endif()
  endif()
endif()
