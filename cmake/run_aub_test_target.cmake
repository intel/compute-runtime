#
# Copyright (C) 2020-2025 Intel Corporation
#
# SPDX-License-Identifier: MIT
#

string(REPLACE "/" ";" aub_test_config ${aub_test_config})
list(GET aub_test_config 0 product)
list(GET aub_test_config 1 slices)
list(GET aub_test_config 2 subslices)
list(GET aub_test_config 3 eu_per_ss)
list(GET aub_test_config 4 revision_id)
list(LENGTH aub_test_config aub_test_config_num_entries)
if(${aub_test_config_num_entries} GREATER_EQUAL 6)
  list(GET aub_test_config 5 device_id)
else()
  set(device_id 0)
endif()

if(NOT TARGET aub_tests)
  add_custom_target(aub_tests)
endif()

add_custom_target(run_${product}_${revision_id}_aub_tests ALL)
add_dependencies(run_${product}_${revision_id}_aub_tests aub_tests)

if(NOT NEO_SKIP_OCL_UNIT_TESTS OR NOT NEO_SKIP_L0_UNIT_TESTS)
  add_dependencies(run_${product}_${revision_id}_aub_tests prepare_test_kernels_for_shared)

  add_dependencies(run_aub_tests run_${product}_${revision_id}_aub_tests)
  set_target_properties(run_${product}_${revision_id}_aub_tests PROPERTIES FOLDER "${AUB_TESTS_TARGETS_FOLDER}/${product}/${revision_id}")

  set(aub_tests_options "")
  if(NOT ${AUB_DUMP_BUFFER_FORMAT} STREQUAL "")
    list(APPEND aub_tests_options --dump_buffer_format)
    list(APPEND aub_tests_options ${AUB_DUMP_BUFFER_FORMAT})
  endif()
  if(NOT ${AUB_DUMP_IMAGE_FORMAT} STREQUAL "")
    list(APPEND aub_tests_options --dump_image_format)
    list(APPEND aub_tests_options ${AUB_DUMP_IMAGE_FORMAT})
  endif()

  set(aubstream_mode_flag "")
  if(DEFINED NEO_GENERATE_AUBS_FOR)
    set(aubstream_mode_flag "--null_aubstream")
    foreach(product_with_aubs ${NEO_GENERATE_AUBS_FOR})
      string(TOLOWER ${product_with_aubs} product_with_aubs_lower)
      if(${product_with_aubs_lower} STREQUAL ${product})
        set(aubstream_mode_flag "")
        string(TOUPPER ${product} product_upper)
        set_property(GLOBAL APPEND PROPERTY NEO_PLATFORMS_FOR_AUB_GENERATION "${product_upper} ")
        break()
      endif()

    endforeach()
  endif()
  if(NOT ${aubstream_mode_flag} STREQUAL "")
    list(APPEND aub_tests_options ${aubstream_mode_flag})
  endif()

  add_custom_command(
                     TARGET run_${product}_${revision_id}_aub_tests
                     POST_BUILD
                     COMMAND WORKING_DIRECTORY ${TargetDir}
                     COMMAND echo re-creating working directory for ${product}/${revision_id} AUBs generation...
                     COMMAND ${CMAKE_COMMAND} -E remove_directory ${TargetDir}/${product}_aub/${revision_id}
                     COMMAND ${CMAKE_COMMAND} -E make_directory ${TargetDir}/${product}_aub/${revision_id}
                     COMMAND ${CMAKE_COMMAND} -E make_directory ${TargetDir}/${product}_aub/${revision_id}/aub_out
  )

endif()

if(TARGET igdrcl_aub_tests)
  add_dependencies(aub_tests igdrcl_aub_tests)
  add_dependencies(run_${product}_${revision_id}_aub_tests prepare_test_kernels_for_ocl)

  if(WIN32 OR NOT DEFINED NEO__GMM_LIBRARY_PATH)
    set(aub_test_cmd_prefix $<TARGET_FILE:igdrcl_aub_tests>)
  else()
    set(aub_test_cmd_prefix ${CMAKE_COMMAND} -E env "LD_LIBRARY_PATH=${NEO__GMM_LIBRARY_PATH}" "IGDRCL_TEST_SELF_EXEC=off" ${NEO_RUN_INTERCEPTOR_LIST} $<TARGET_FILE:igdrcl_aub_tests>)
  endif()

  unset(GTEST_OUTPUT)
  if(DEFINED GTEST_OUTPUT_DIR)
    set(GTEST_OUTPUT "--gtest_output=json:${GTEST_OUTPUT_DIR}/ocl-${product}-${revision_id}-aub_tests-results.json")
  endif()

  add_custom_command(
                     TARGET run_${product}_${revision_id}_aub_tests
                     POST_BUILD
                     COMMAND WORKING_DIRECTORY ${TargetDir}
                     COMMAND echo Running AUB generation for ${product} in ${TargetDir}/${product}_aub
                     COMMAND ${aub_test_cmd_prefix} --product ${product} --slices ${slices} --subslices ${subslices} --eu_per_ss ${eu_per_ss} --gtest_repeat=1 ${aub_tests_options} ${NEO_TESTS_LISTENER_OPTION} ${GTEST_OUTPUT} --rev_id ${revision_id} --dev_id ${device_id}
  )
endif()

if(TARGET ze_intel_gpu_aub_tests)
  add_dependencies(aub_tests ze_intel_gpu_aub_tests)
  add_dependencies(run_${product}_${revision_id}_aub_tests prepare_test_kernels_for_l0)

  if(WIN32 OR NOT DEFINED NEO__GMM_LIBRARY_PATH)
    set(l0_aub_test_cmd_prefix $<TARGET_FILE:ze_intel_gpu_aub_tests>)
  else()
    set(l0_aub_test_cmd_prefix ${CMAKE_COMMAND} -E env "LD_LIBRARY_PATH=${NEO__GMM_LIBRARY_PATH}" ${NEO_RUN_INTERCEPTOR_LIST} $<TARGET_FILE:ze_intel_gpu_aub_tests>)
  endif()

  unset(GTEST_OUTPUT)
  if(DEFINED GTEST_OUTPUT_DIR)
    set(GTEST_OUTPUT "--gtest_output=json:${GTEST_OUTPUT_DIR}/ze_intel_gpu-${product}-${revision_id}-aub_tests-results.json")
  endif()

  add_custom_command(
                     TARGET run_${product}_${revision_id}_aub_tests
                     POST_BUILD
                     COMMAND WORKING_DIRECTORY ${TargetDir}
                     COMMAND echo Running Level Zero AUB generation for ${product} in ${TargetDir}/${product}_aub
                     COMMAND ${l0_aub_test_cmd_prefix} --product ${product} --slices ${slices} --subslices ${subslices} --eu_per_ss ${eu_per_ss} --gtest_repeat=1 ${aub_tests_options} ${GTEST_OUTPUT} --rev_id ${revision_id} --dev_id ${device_id}
  )
endif()

if(NEO_SKIP_AUB_TESTS_RUN)
  set_target_properties(run_${product}_${revision_id}_aub_tests PROPERTIES
                        EXCLUDE_FROM_DEFAULT_BUILD TRUE
                        EXCLUDE_FROM_ALL TRUE
  )
endif()
