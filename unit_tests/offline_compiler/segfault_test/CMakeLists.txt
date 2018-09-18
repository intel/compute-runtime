#
# Copyright (C) 2018 Intel Corporation
#
# SPDX-License-Identifier: MIT
#

set(CLOC_SEGFAULT_TEST_SOURCES
  ${CMAKE_CURRENT_SOURCE_DIR}/CMakeLists.txt
  ${CMAKE_CURRENT_SOURCE_DIR}/segfault_helper.h
  ${CMAKE_CURRENT_SOURCE_DIR}/main.cpp
  ${IGDRCL_SOURCE_DIR}/runtime/helpers/abort.cpp
  ${IGDRCL_SOURCE_DIR}/runtime/os_interface/os_library.h
  ${IGDRCL_SOURCE_DIR}/unit_tests/helpers/debug_helpers.cpp
)

if(WIN32)
  list(APPEND CLOC_SEGFAULT_TEST_SOURCES
    ${CMAKE_CURRENT_SOURCE_DIR}/windows/safety_guard_caller_windows.cpp
    ${IGDRCL_SOURCE_DIR}/offline_compiler/utilities/windows/safety_guard_windows.h
    ${IGDRCL_SOURCE_DIR}/offline_compiler/utilities/windows/seh_exception.cpp
    ${IGDRCL_SOURCE_DIR}/offline_compiler/utilities/windows/seh_exception.h
    ${IGDRCL_SOURCE_DIR}/runtime/os_interface/windows/os_library.cpp
  )
else()
  list(APPEND CLOC_SEGFAULT_TEST_SOURCES
    ${CMAKE_CURRENT_SOURCE_DIR}/linux/safety_guard_caller_linux.cpp
    ${IGDRCL_SOURCE_DIR}/runtime/os_interface/linux/os_library.cpp
  )
endif()

add_executable(cloc_segfault_test ${CLOC_SEGFAULT_TEST_SOURCES})
target_link_libraries(cloc_segfault_test gmock-gtest)
if(MSVC)
  target_compile_options(cloc_segfault_test PRIVATE /Zi)
  set_property(TARGET cloc_segfault_test APPEND PROPERTY LINK_FLAGS /DEBUG)
  target_link_libraries(cloc_segfault_test dbghelp)
endif()
if(UNIX)
  target_link_libraries(cloc_segfault_test dl pthread)
endif()

set(CLOC_SEGFAULT_TEST_INCLUDES
  ${IGDRCL_SOURCE_DIR}
  ${THIRD_PARTY_DIR}
)

if(CMAKE_COMPILER_IS_GNUCC)
  target_compile_definitions(cloc_segfault_test PRIVATE SKIP_SEGFAULT_TEST=1)
endif()

get_property(CLOC_FOLDER TARGET cloc PROPERTY FOLDER)
set_property(TARGET cloc_segfault_test PROPERTY FOLDER ${CLOC_FOLDER})

target_include_directories(cloc_segfault_test BEFORE PRIVATE ${CLOC_SEGFAULT_TEST_INCLUDES})
create_project_source_tree(cloc_segfault_test ${IGDRCL_SOURCE_DIR}/offline_compiler ${IGDRCL_SOURCE_DIR}/runtime ${IGDRCL_SOURCE_DIR}/unit_tests)

add_custom_target(run_cloc_segfault_test ALL
  DEPENDS cloc_segfault_test
)

add_custom_command(
  TARGET run_cloc_segfault_test
  POST_BUILD
  COMMAND echo Running cloc_segfault_test
  COMMAND cloc_segfault_test
  WORKING_DIRECTORY ${TargetDir}
)

set_property(TARGET run_cloc_segfault_test PROPERTY FOLDER ${CLOC_FOLDER})
