#
# Copyright (C) 2020-2025 Intel Corporation
#
# SPDX-License-Identifier: MIT
#

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

    target_include_directories(${TEST_NAME} PUBLIC
                               ${NEO_SOURCE_DIR}/level_zero/include
    )

    if(BUILD_LEVEL_ZERO_LOADER)
      add_dependencies(${TEST_NAME} ze_loader)
      target_link_libraries(${TEST_NAME} ${NEO_BINARY_DIR}/lib/libze_loader.so pthread)
    else()
      target_link_libraries(${TEST_NAME} PUBLIC ${TARGET_NAME_L0} pthread)
    endif()
    set_target_properties(${TEST_NAME} PROPERTIES FOLDER ${L0_BLACK_BOX_TEST_PROJECT_FOLDER})
  endforeach()
endif()
if(WIN32)
  set(L0_BLACK_BOX_TEST_PROJECT_FOLDER "ze_intel_gpu/sysman_black_box_tests")
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

    target_include_directories(${TEST_NAME} PUBLIC
                               ${NEO_SOURCE_DIR}/level_zero/include
    )

    if(BUILD_LEVEL_ZERO_LOADER)
      add_dependencies(${TEST_NAME} ze_loader)
      target_link_libraries(${TEST_NAME} ${NEO_BINARY_DIR}/lib/libze_loader.dll)
    else()
      target_link_libraries(${TEST_NAME} PUBLIC ${TARGET_NAME_L0})
    endif()
    set_target_properties(${TEST_NAME} PROPERTIES FOLDER ${L0_BLACK_BOX_TEST_PROJECT_FOLDER})
  endforeach()
endif()
add_subdirectories()
