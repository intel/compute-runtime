#
# Copyright (C) 2026 Intel Corporation
#
# SPDX-License-Identifier: MIT
#

set(L0_BLACK_BOX_GRAPH_TEST_PROJECT_FOLDER "ze_intel_gpu/black_box_tests/graph_samples")

set(L0_BLACK_BOX_TEST_GRAPH_SHARED_LIB "zello_graph_shared_lib")

add_library(${L0_BLACK_BOX_TEST_GRAPH_SHARED_LIB} STATIC
            ${CMAKE_CURRENT_SOURCE_DIR}/CMakeLists.txt
            ${CMAKE_CURRENT_SOURCE_DIR}/zello_graph_test_helper.h
            ${CMAKE_CURRENT_SOURCE_DIR}/zello_graph_test_helper.cpp
)

add_dependencies(${L0_BLACK_BOX_TEST_GRAPH_SHARED_LIB} ${L0_BLACK_BOX_TEST_SHARED_LIB})
target_link_libraries(${L0_BLACK_BOX_TEST_GRAPH_SHARED_LIB} PUBLIC ${L0_BLACK_BOX_TEST_SHARED_LIB})

set_target_properties(${L0_BLACK_BOX_TEST_GRAPH_SHARED_LIB} PROPERTIES FOLDER ${L0_BLACK_BOX_GRAPH_TEST_PROJECT_FOLDER})

set(TEST_TARGETS
    zello_graph
    zello_graph_gemm_submit
    zello_mutable_graph
)

foreach(TEST_NAME ${TEST_TARGETS})
  add_executable(${TEST_NAME} ${TEST_NAME}.cpp)

  set_target_properties(${TEST_NAME}
                        PROPERTIES
                        VS_DEBUGGER_COMMAND "$(TargetPath)"
                        VS_DEBUGGER_COMMAND_ARGUMENTS ""
                        VS_DEBUGGER_WORKING_DIRECTORY "${TargetDir}"
  )

  target_link_libraries(${TEST_NAME} PUBLIC ${L0_LOADER_LIBRARY})

  set_target_properties(${TEST_NAME} PROPERTIES FOLDER ${L0_BLACK_BOX_GRAPH_TEST_PROJECT_FOLDER})

  add_dependencies(${TEST_NAME} ${L0_BLACK_BOX_TEST_SHARED_LIB} ${L0_BLACK_BOX_TEST_GRAPH_SHARED_LIB})
  target_link_libraries(${TEST_NAME} PUBLIC ${L0_BLACK_BOX_TEST_SHARED_LIB} ${L0_BLACK_BOX_TEST_GRAPH_SHARED_LIB})
endforeach()
