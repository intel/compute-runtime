#
# Copyright (C) 2017-2019 Intel Corporation
#
# SPDX-License-Identifier: MIT
#

function(neo_run_aub_target gen gen_name product slices subslices eu_per_ss options)
    add_custom_command(
        TARGET run_${gen}_aub_tests
        POST_BUILD
        COMMAND WORKING_DIRECTORY ${TargetDir}
        COMMAND echo re-creating working directory for ${gen_name} AUBs generation...
        COMMAND ${CMAKE_COMMAND} -E remove_directory ${TargetDir}/${gen}_aub
        COMMAND ${CMAKE_COMMAND} -E make_directory ${TargetDir}/${gen}_aub
        COMMAND ${CMAKE_COMMAND} -E make_directory ${TargetDir}/${gen}_aub/aub_out
        COMMAND ${CMAKE_COMMAND} -E make_directory ${TargetDir}/${gen}_aub/cl_cache
    )

    if(WIN32 OR NOT DEFINED IGDRCL__GMM_LIBRARY_PATH)
        set(aub_cmd_prefix $<TARGET_FILE:igdrcl_aub_tests>)
    else()
        set(aub_cmd_prefix LD_LIBRARY_PATH=${IGDRCL__GMM_LIBRARY_PATH} IGDRCL_TEST_SELF_EXEC=off $<TARGET_FILE:igdrcl_aub_tests>)
    endif()

    add_custom_command(
        TARGET run_${gen}_aub_tests
        POST_BUILD
        COMMAND WORKING_DIRECTORY ${TargetDir}
        COMMAND echo Running AUB generation for ${gen_name} in ${TargetDir}/${gen}_aub
        COMMAND ${CMAKE_COMMAND} -E env GTEST_OUTPUT=xml:${GTEST_XML_OUTPUT_DIR}/aub_test_details_${product}_${slices}_${subslices}_${eu_per_ss}.xml ${aub_cmd_prefix} --product ${product} --slices ${slices} --subslices ${subslices} --eu_per_ss ${eu_per_ss} --gtest_repeat=1 ${options} ${IGDRCL_TESTS_LISTENER_OPTION}
    )

    if(DO_NOT_RUN_AUB_TESTS)
        set_target_properties(run_${gen}_aub_tests PROPERTIES
            EXCLUDE_FROM_DEFAULT_BUILD TRUE
            EXCLUDE_FROM_ALL TRUE
        )
    endif()
endfunction()
