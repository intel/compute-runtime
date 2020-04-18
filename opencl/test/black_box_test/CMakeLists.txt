#
# Copyright (C) 2020 Intel Corporation
#
# SPDX-License-Identifier: MIT
#

if("${CMAKE_BUILD_TYPE}" STREQUAL "Debug" AND WIN32)
    set(OPENCL_BLACK_BOX_TEST_PROJECT_FOLDER "opencl runtime/black_box_tests")
    set(TEST_NAME hello_world_opencl)

    add_executable(${TEST_NAME} hello_world_opencl.cpp)

    set_target_properties(${TEST_NAME}
        PROPERTIES
        VS_DEBUGGER_COMMAND "$(TargetPath)"
        VS_DEBUGGER_COMMAND_ARGUMENTS ""
        VS_DEBUGGER_WORKING_DIRECTORY "$(OutDir)"
    )

    add_dependencies(${TEST_NAME} ${NEO_DYNAMIC_LIB_NAME})
    target_link_libraries(${TEST_NAME} PUBLIC  ${NEO_DYNAMIC_LIB_NAME})
    set_target_properties(${TEST_NAME} PROPERTIES FOLDER ${OPENCL_BLACK_BOX_TEST_PROJECT_FOLDER})
endif()