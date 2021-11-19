#
# Copyright (C) 2020-2021 Intel Corporation
#
# SPDX-License-Identifier: MIT
#

string(REPLACE "/" ";" unit_test_config ${unit_test_config})
list(GET unit_test_config 0 product)
list(GET unit_test_config 1 slices)
list(GET unit_test_config 2 subslices)
list(GET unit_test_config 3 eu_per_ss)
list(GET unit_test_config 4 revision_id)

add_custom_target(run_${product}_${revision_id}_unit_tests ALL DEPENDS unit_tests)
set_target_properties(run_${product}_${revision_id}_unit_tests PROPERTIES FOLDER "${PLATFORM_SPECIFIC_TEST_TARGETS_FOLDER}/${product}/${revision_id}")
if(NOT NEO_SKIP_SHARED_UNIT_TESTS)
  add_custom_command(
                     TARGET run_${product}_${revision_id}_unit_tests
                     POST_BUILD
                     COMMAND WORKING_DIRECTORY ${TargetDir}
                     COMMAND echo Running neo_shared_tests ${target} ${slices}x${subslices}x${eu_per_ss} in ${TargetDir}/${product}
                     COMMAND ${NEO_RUN_INTERCEPTOR_LIST} $<TARGET_FILE:neo_shared_tests> --product ${product} --slices ${slices} --subslices ${subslices} --eu_per_ss ${eu_per_ss} ${GTEST_EXCEPTION_OPTIONS} --gtest_repeat=${GTEST_REPEAT} ${GTEST_SHUFFLE} ${NEO_TESTS_LISTENER_OPTION} ${GTEST_FILTER_OPTION} --rev_id ${revision_id}
  )

endif()
if(NOT NEO_SKIP_OCL_UNIT_TESTS)
  add_custom_command(
                     TARGET run_${product}_${revision_id}_unit_tests
                     POST_BUILD
                     COMMAND WORKING_DIRECTORY ${TargetDir}
                     COMMAND echo Running igdrcl_tests ${target} ${slices}x${subslices}x${eu_per_ss} in ${TargetDir}/${product}
                     COMMAND ${GTEST_ENV} ${NEO_RUN_INTERCEPTOR_LIST} $<TARGET_FILE:igdrcl_tests> --product ${product} --slices ${slices} --subslices ${subslices} --eu_per_ss ${eu_per_ss} ${GTEST_EXCEPTION_OPTIONS} --gtest_repeat=${GTEST_REPEAT} ${GTEST_SHUFFLE} ${NEO_TESTS_LISTENER_OPTION} ${GTEST_FILTER_OPTION} --rev_id ${revision_id}
  )

  if(WIN32 AND ${CMAKE_BUILD_TYPE} STREQUAL "Debug" AND "${IGDRCL_OPTION__BITS}" STREQUAL "64" AND APPVERIFIER_ALLOWED)
    add_custom_command(
                       TARGET run_${product}_${revision_id}_unit_tests
                       POST_BUILD
                       COMMAND WORKING_DIRECTORY ${TargetDir}
                       COMMAND echo Running igdrcl_tests with App Verifier
                       COMMAND ${NEO_SOURCE_DIR}/scripts/verify.bat $<TARGET_FILE:igdrcl_tests> --product ${product} --slices ${slices} --subslices ${subslices} --eu_per_ss ${eu_per_ss} ${GTEST_EXCEPTION_OPTIONS} ${NEO_TESTS_LISTENER_OPTION} ${GTEST_FILTER_OPTION} --rev_id ${revision_id}
                       COMMAND echo App Verifier returned: %errorLevel%
    )
  endif()
endif()

if(NOT NEO_SKIP_L0_UNIT_TESTS AND BUILD_WITH_L0)
  add_custom_command(
                     TARGET run_${product}_${revision_id}_unit_tests
                     POST_BUILD
                     COMMAND WORKING_DIRECTORY ${TargetDir}
                     COMMAND echo Running ze_intel_gpu_core_tests ${target} ${slices}x${subslices}x${eu_per_ss} in ${TargetDir}/${product}
                     COMMAND ${NEO_RUN_INTERCEPTOR_LIST} $<TARGET_FILE:ze_intel_gpu_core_tests> --product ${product} --slices ${slices} --subslices ${subslices} --eu_per_ss ${eu_per_ss} ${GTEST_EXCEPTION_OPTIONS} --gtest_repeat=${GTEST_REPEAT} ${GTEST_SHUFFLE} ${NEO_TESTS_LISTENER_OPTION} ${GTEST_FILTER_OPTION} --rev_id ${revision_id}
                     COMMAND echo Running ze_intel_gpu_tools_tests ${target} ${slices}x${subslices}x${eu_per_ss} in ${TargetDir}/${product}
                     COMMAND ${NEO_RUN_INTERCEPTOR_LIST} $<TARGET_FILE:ze_intel_gpu_tools_tests> --product ${product} --slices ${slices} --subslices ${subslices} --eu_per_ss ${eu_per_ss} ${GTEST_EXCEPTION_OPTIONS} --gtest_repeat=${GTEST_REPEAT} ${GTEST_SHUFFLE} ${NEO_TESTS_LISTENER_OPTION} ${GTEST_FILTER_OPTION} --rev_id ${revision_id}
                     COMMAND echo Running ze_intel_gpu_exp_tests ${target} ${slices}x${subslices}x${eu_per_ss} in ${TargetDir}/${product}
                     COMMAND ${NEO_RUN_INTERCEPTOR_LIST} $<TARGET_FILE:ze_intel_gpu_exp_tests> --product ${product} --slices ${slices} --subslices ${subslices} --eu_per_ss ${eu_per_ss} ${GTEST_EXCEPTION_OPTIONS} --gtest_repeat=${GTEST_REPEAT} ${GTEST_SHUFFLE} ${NEO_TESTS_LISTENER_OPTION} ${GTEST_FILTER_OPTION} --rev_id ${revision_id}
  )
endif()

add_dependencies(run_unit_tests run_${product}_${revision_id}_unit_tests)
