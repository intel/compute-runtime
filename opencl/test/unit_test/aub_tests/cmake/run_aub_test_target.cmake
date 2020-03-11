#
# Copyright (C) 2020 Intel Corporation
#
# SPDX-License-Identifier: MIT
#

string(REPLACE "/" ";" aub_test_config ${aub_test_config})
list(GET aub_test_config 0 product)
list(GET aub_test_config 1 slices)
list(GET aub_test_config 2 subslices)
list(GET aub_test_config 3 eu_per_ss)

add_custom_target(run_${product}_aub_tests ALL DEPENDS copy_test_files_${product} prepare_test_kernels)
add_dependencies(run_aub_tests run_${product}_aub_tests)
set_target_properties(run_${product}_aub_tests PROPERTIES FOLDER "${PLATFORM_SPECIFIC_TEST_TARGETS_FOLDER}/${product}")

if(WIN32)
  add_dependencies(run_${product}_aub_tests mock_gdi)
endif()

set(aub_tests_options "")
if (NOT ${AUB_DUMP_BUFFER_FORMAT} STREQUAL "")
 list(APPEND aub_tests_options --dump_buffer_format)
  list(APPEND aub_tests_options ${AUB_DUMP_BUFFER_FORMAT})
endif()
if (NOT ${AUB_DUMP_IMAGE_FORMAT} STREQUAL "")
  list(APPEND aub_tests_options --dump_image_format)
  list(APPEND aub_tests_options ${AUB_DUMP_IMAGE_FORMAT})
endif()

add_custom_command(
  TARGET run_${product}_aub_tests
  POST_BUILD
  COMMAND WORKING_DIRECTORY ${TargetDir}
  COMMAND echo re-creating working directory for ${product} AUBs generation...
  COMMAND ${CMAKE_COMMAND} -E remove_directory ${TargetDir}/${product}_aub
  COMMAND ${CMAKE_COMMAND} -E make_directory ${TargetDir}/${product}_aub
  COMMAND ${CMAKE_COMMAND} -E make_directory ${TargetDir}/${product}_aub/aub_out
  COMMAND ${CMAKE_COMMAND} -E make_directory ${TargetDir}/${product}_aub/cl_cache
)

if(WIN32 OR NOT DEFINED NEO__GMM_LIBRARY_PATH)
  set(aub_test_cmd_prefix $<TARGET_FILE:igdrcl_aub_tests>)
else()
  set(aub_test_cmd_prefix LD_LIBRARY_PATH=${NEO__GMM_LIBRARY_PATH} IGDRCL_TEST_SELF_EXEC=off $<TARGET_FILE:igdrcl_aub_tests>)
endif()

add_custom_command(
  TARGET run_${product}_aub_tests
  POST_BUILD
  COMMAND WORKING_DIRECTORY ${TargetDir}
  COMMAND echo Running AUB generation for ${product} in ${TargetDir}/${product}_aub
  COMMAND ${aub_test_cmd_prefix} --product ${product} --slices ${slices} --subslices ${subslices} --eu_per_ss ${eu_per_ss} --gtest_repeat=1 ${aub_tests_options} ${IGDRCL_TESTS_LISTENER_OPTION}
)

if(DO_NOT_RUN_AUB_TESTS)
  set_target_properties(run_${product}_aub_tests PROPERTIES
    EXCLUDE_FROM_DEFAULT_BUILD TRUE
    EXCLUDE_FROM_ALL TRUE
  )
endif()
