#
# Copyright (C) 2020 Intel Corporation
#
# SPDX-License-Identifier: MIT
#

string(REPLACE "/" ";" unit_test_config ${unit_test_config})
list(GET unit_test_config 0 product)
list(GET unit_test_config 1 slices)
list(GET unit_test_config 2 subslices)
list(GET unit_test_config 3 eu_per_ss)
add_custom_target(run_${product}_unit_tests ALL DEPENDS unit_tests)
set_target_properties(run_${product}_unit_tests PROPERTIES FOLDER "${PLATFORM_SPECIFIC_TEST_TARGETS_FOLDER}/${product}")
if(NOT SKIP_NEO_UNIT_TESTS)
    add_custom_command(
      TARGET run_${product}_unit_tests
      POST_BUILD
      COMMAND WORKING_DIRECTORY ${TargetDir}
      COMMAND echo Running igdrcl_tests ${target} ${slices}x${subslices}x${eu_per_ss} in ${TargetDir}/${product}
      COMMAND $<TARGET_FILE:igdrcl_tests> --product ${product} --slices ${slices} --subslices ${subslices} --eu_per_ss ${eu_per_ss} ${GTEST_EXCEPTION_OPTIONS} --gtest_repeat=${GTEST_REPEAT} ${GTEST_SHUFFLE} ${IGDRCL_TESTS_LISTENER_OPTION} ${GTEST_FILTER_OPTION}
    )

    if(WIN32 AND ${CMAKE_BUILD_TYPE} STREQUAL "Debug" AND "${IGDRCL_OPTION__BITS}" STREQUAL "64" AND APPVERIFIER_ALLOWED)
      add_custom_command(
        TARGET run_${product}_unit_tests
        POST_BUILD
        COMMAND WORKING_DIRECTORY ${TargetDir}
        COMMAND echo Running igdrcl_tests with App Verifier
        COMMAND ${NEO_SOURCE_DIR}/scripts/verify.bat $<TARGET_FILE:igdrcl_tests> --product ${product} --slices ${slices} --subslices ${subslices} --eu_per_ss ${eu_per_ss} ${GTEST_EXCEPTION_OPTIONS} ${IGDRCL_TESTS_LISTENER_OPTION} ${GTEST_FILTER_OPTION}
        COMMAND echo App Verifier returned: %errorLevel%
      )
    endif()
endif()

if(NOT SKIP_L0_UNIT_TESTS AND BUILD_WITH_L0)
    add_custom_command(
      TARGET run_${product}_unit_tests
      POST_BUILD
      COMMAND WORKING_DIRECTORY ${TargetDir}
      COMMAND echo Running ze_intel_gpu_core_tests ${target} ${slices}x${subslices}x${eu_per_ss} in ${TargetDir}/${product}
      COMMAND $<TARGET_FILE:ze_intel_gpu_core_tests> --product ${product} --slices ${slices} --subslices ${subslices} --eu_per_ss ${eu_per_ss} ${GTEST_EXCEPTION_OPTIONS} --gtest_repeat=${GTEST_REPEAT} ${GTEST_SHUFFLE} ${IGDRCL_TESTS_LISTENER_OPTION} ${GTEST_FILTER_OPTION}
    )
endif()

add_dependencies(run_unit_tests run_${product}_unit_tests)
