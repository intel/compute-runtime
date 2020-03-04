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
add_custom_target(run_${product}_unit_tests)
add_dependencies(run_${product}_unit_tests unit_tests)
neo_copy_test_files(copy_test_files_${product} ${product})
add_dependencies(unit_tests copy_test_files_${product})
set_target_properties(run_${product}_unit_tests PROPERTIES FOLDER "${OPENCL_PLATFORM_SPECIFIC_TEST_TARGETS_FOLDER}/${product}")
add_custom_command(
  TARGET run_${product}_unit_tests
  POST_BUILD
  COMMAND WORKING_DIRECTORY ${TargetDir}
  COMMAND echo Running igdrcl_tests ${target} ${slices}x${subslices}x${eu_per_ss} in ${TargetDir}/${product}
  COMMAND $<TARGET_FILE:igdrcl_tests> --product ${product} --slices ${slices} --subslices ${subslices} --eu_per_ss ${eu_per_ss} ${GTEST_EXCEPTION_OPTIONS} --gtest_repeat=${GTEST_REPEAT} ${GTEST_SHUFFLE} ${IGDRCL_TESTS_LISTENER_OPTION} ${GTEST_FILTER_OPTION}
)
add_dependencies(run_unit_tests run_${product}_unit_tests)

if(${CMAKE_BUILD_TYPE} STREQUAL "Debug" AND "${IGDRCL_OPTION__BITS}" STREQUAL "64" AND appverified_allowed)
  add_custom_command(
    TARGET run_${product}_unit_tests
    POST_BUILD
    COMMAND echo copying test verify.bat file from ${OPENCL_UNIT_TEST_DIR} to ${TargetDir}
    COMMAND ${CMAKE_COMMAND} -E copy_if_different ${OPENCL_UNIT_TEST_DIR}/verify.bat ${TargetDir}/verify.bat
    COMMAND WORKING_DIRECTORY ${TargetDir}
    COMMAND echo Running igdrcl_tests with App Verifier
    COMMAND ${TargetDir}/verify.bat --product ${product} --slices ${slices} --subslices ${subslices} --eu_per_ss ${eu_per_ss} ${GTEST_EXCEPTION_OPTIONS} ${IGDRCL_TESTS_LISTENER_OPTION} ${GTEST_FILTER_OPTION}
    COMMAND echo App Verifier returned: %errorLevel%
  )
endif()