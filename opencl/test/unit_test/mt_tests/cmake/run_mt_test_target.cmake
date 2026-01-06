#
# Copyright (C) 2020-2025 Intel Corporation
#
# SPDX-License-Identifier: MIT
#

string(REPLACE "/" ";" mt_test_config ${mt_test_config})
list(GET mt_test_config 0 product)
list(GET mt_test_config 1 slices)
list(GET mt_test_config 2 subslices)
list(GET mt_test_config 3 eu_per_ss)
list(GET mt_test_config 4 revision_id)

add_custom_target(run_${product}_${revision_id}_mt_unit_tests DEPENDS igdrcl_mt_tests)

unset(GTEST_OUTPUT)
if(DEFINED GTEST_OUTPUT_DIR)
  set(GTEST_OUTPUT "--gtest_output=json:${GTEST_OUTPUT_DIR}/ocl-${product}-${revision_id}-mt_unit_tests-results.json")
endif()

add_dependencies(run_mt_unit_tests run_${product}_${revision_id}_mt_unit_tests)
set_target_properties(run_${product}_${revision_id}_mt_unit_tests PROPERTIES FOLDER "${PLATFORM_SPECIFIC_TEST_TARGETS_FOLDER}/${product}/${revision_id}")

add_custom_command(
                   TARGET run_${product}_${revision_id}_mt_unit_tests
                   POST_BUILD
                   COMMAND WORKING_DIRECTORY ${TargetDir}
                   COMMAND echo "Running igdrcl_mt_tests ${product} ${slices}x${subslices}x${eu_per_ss}"
                   COMMAND igdrcl_mt_tests --product ${product} --slices ${slices} --subslices ${subslices} --eu_per_ss ${eu_per_ss} --gtest_repeat=${GTEST_REPEAT} ${GTEST_OUTPUT} ${NEO_TESTS_LISTENER_OPTION} --rev_id ${revision_id}
)

add_dependencies(run_${product}_${revision_id}_mt_unit_tests prepare_test_kernels_for_ocl prepare_test_kernels_for_shared)
