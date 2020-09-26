#
# Copyright (C) 2020 Intel Corporation
#
# SPDX-License-Identifier: MIT
#

if("${CMAKE_BUILD_TYPE}" STREQUAL "Debug")
  if(UNIX)
    set(L0_BLACK_BOX_TEST_PROJECT_FOLDER "ze_intel_gpu/black_box_tests")
    set(TEST_TARGETS
        zello_sysman
    )

    foreach(TEST_NAME ${TEST_TARGETS})
      add_executable(${TEST_NAME} ${TEST_NAME}.cpp)

      set_target_properties(${TEST_NAME}
                            PROPERTIES
                            VS_DEBUGGER_COMMAND "$(TargetPath)"
                            VS_DEBUGGER_COMMAND_ARGUMENTS ""
                            VS_DEBUGGER_WORKING_DIRECTORY "$(OutputPath)"
      )

      find_library(ZE_LOADER ze_loader)
      if(ZE_LOADER)
        target_link_libraries(${TEST_NAME} ze_loader)
      else()
        set_target_properties(${TEST_NAME} PROPERTIES EXCLUDE_FROM_ALL TRUE)
      endif()
      set_target_properties(${TEST_NAME} PROPERTIES FOLDER ${L0_BLACK_BOX_TEST_PROJECT_FOLDER})
    endforeach()
  endif()
endif()
