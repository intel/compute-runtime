#
# Copyright (C) 2020 Intel Corporation
#
# SPDX-License-Identifier: MIT
#

string(REPLACE "/" ";" mt_test_config ${mt_test_config})
list(GET mt_test_config 0 product)
list(GET mt_test_config 1 slices)
list(GET mt_test_config 2 subslices)
list(GET mt_test_config 3 eu_per_ss)

add_custom_target(run_${product}_mt_unit_tests DEPENDS igdrcl_mt_tests)
if(NOT WIN32)
  add_dependencies(run_${product}_mt_unit_tests copy_test_files_${product})
endif()

add_dependencies(run_mt_unit_tests run_${product}_mt_unit_tests)
set_target_properties(run_${product}_mt_unit_tests PROPERTIES FOLDER "${OPENCL_PLATFORM_SPECIFIC_TEST_TARGETS_FOLDER}/${product}")

add_custom_command(
  TARGET run_${product}_mt_unit_tests
  POST_BUILD
  COMMAND WORKING_DIRECTORY ${TargetDir}
  COMMAND echo "Running igdrcl_mt_tests ${product} ${slices}x${subslices}x${eu_per_ss}"
  COMMAND igdrcl_mt_tests --product ${product} --slices ${slices} --subslices ${subslices} --eu_per_ss ${eu_per_ss} --gtest_repeat=${GTEST_REPEAT} ${igdrcl_mt_tests_LISTENER_OPTION}
)

add_dependencies(run_${product}_mt_unit_tests prepare_test_kernels)