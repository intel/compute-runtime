#
# Copyright (C) 2020-2025 Intel Corporation
#
# SPDX-License-Identifier: MIT
#

if("${CMAKE_BUILD_TYPE}" STREQUAL "Debug")
  set(OPENCL_BLACK_BOX_TEST_PROJECT_FOLDER "opencl runtime/black_box_tests")
  set(TEST_TARGETS
      hello_world_opencl
      hello_world_opencl_tracing
  )

  foreach(TEST_NAME ${TEST_TARGETS})
    add_executable(${TEST_NAME} "${TEST_NAME}.cpp")

    set_target_properties(${TEST_NAME}
                          PROPERTIES
                          VS_DEBUGGER_COMMAND "$(TargetPath)"
                          VS_DEBUGGER_COMMAND_ARGUMENTS ""
                          VS_DEBUGGER_WORKING_DIRECTORY "$(OutDir)"
    )

    add_dependencies(${TEST_NAME} ${NEO_DYNAMIC_LIB_NAME})
    target_include_directories(${TEST_NAME} PRIVATE ${KHRONOS_HEADERS_DIR})
    set_target_properties(${TEST_NAME} PROPERTIES FOLDER ${OPENCL_BLACK_BOX_TEST_PROJECT_FOLDER})

    target_link_libraries(${TEST_NAME} PUBLIC ocloc_lib)
    target_include_directories(${TEST_NAME} PUBLIC "${NEO_SOURCE_DIR}/shared/offline_compiler/source")

    if(UNIX)
      find_package(OpenCL QUIET)

      if(NOT ${OpenCL_FOUND})
        message(STATUS "Failed to find OpenCL package")
        set_target_properties(${TEST_NAME} PROPERTIES EXCLUDE_FROM_ALL TRUE)
      else()
        target_link_libraries(${TEST_NAME} PUBLIC ${OpenCL_LIBRARIES})
      endif()
    else()
      target_link_libraries(${TEST_NAME} PUBLIC ${NEO_DYNAMIC_LIB_NAME})
    endif()
  endforeach()
endif()

add_subdirectories()