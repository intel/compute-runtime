# Copyright (c) 2017, Intel Corporation
#
# Permission is hereby granted, free of charge, to any person obtaining a
# copy of this software and associated documentation files (the "Software"),
# to deal in the Software without restriction, including without limitation
# the rights to use, copy, modify, merge, publish, distribute, sublicense,
# and/or sell copies of the Software, and to permit persons to whom the
# Software is furnished to do so, subject to the following conditions:
#
# The above copyright notice and this permission notice shall be included
# in all copies or substantial portions of the Software.
#
# THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS
# OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
# FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL
# THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR
# OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE,
# ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR
# OTHER DEALINGS IN THE SOFTWARE.
function(neo_run_aub_target gen gen_name product slices subslices eu_per_ss)
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

    add_custom_command(
        TARGET run_${gen}_aub_tests
        POST_BUILD
        COMMAND WORKING_DIRECTORY ${TargetDir}
        COMMAND echo Running AUB generation for ${gen_name} in ${TargetDir}/${gen}_aub
        COMMAND igdrcl_aub_tests --product ${product} --slices ${slices} --subslices ${subslices} --eu_per_ss ${eu_per_ss} --gtest_repeat=1 ${IGDRCL_TESTS_LISTENER_OPTION}
    )

    if(DO_NOT_RUN_AUB_TESTS)
        set_target_properties(run_${gen}_aub_tests PROPERTIES
            EXCLUDE_FROM_DEFAULT_BUILD TRUE
            EXCLUDE_FROM_ALL TRUE
        )
    endif()
endfunction()
