#
# Copyright (C) 2020 Intel Corporation
#
# SPDX-License-Identifier: MIT
#

if("${CMAKE_BUILD_TYPE}" STREQUAL "Debug")
  set(L0_BLACK_BOX_TEST_PROJECT_FOLDER "ze_intel_gpu/black_box_tests")
  set(TEST_TARGETS
      zello_world_gpu
      zello_timestamp
  )

  foreach(TEST_NAME ${TEST_TARGETS})
    add_executable(${TEST_NAME} ${TEST_NAME}.cpp)

    set_target_properties(${TEST_NAME}
                          PROPERTIES
                          VS_DEBUGGER_COMMAND "$(TargetPath)"
                          VS_DEBUGGER_COMMAND_ARGUMENTS ""
                          VS_DEBUGGER_WORKING_DIRECTORY "$(OutputPath)"
    )

    add_dependencies(${TEST_NAME} ${TARGET_NAME_L0})
    target_link_libraries(${TEST_NAME} PUBLIC ${TARGET_NAME_L0})

    set_target_properties(${TEST_NAME} PROPERTIES FOLDER ${L0_BLACK_BOX_TEST_PROJECT_FOLDER})
  endforeach()
endif()
