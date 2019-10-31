#
# Copyright (C) 2018-2019 Intel Corporation
#
# SPDX-License-Identifier: MIT
#

set(CLOC_SEGFAULT_TEST_SOURCES
  ${CMAKE_CURRENT_SOURCE_DIR}/CMakeLists.txt
  ${CMAKE_CURRENT_SOURCE_DIR}/segfault_helper.h
  ${CMAKE_CURRENT_SOURCE_DIR}/main.cpp
  ${NEO_SOURCE_DIR}/core/helpers/abort.cpp
  ${NEO_SOURCE_DIR}/runtime/os_interface/os_library.h
  ${NEO_SOURCE_DIR}/unit_tests/helpers/debug_helpers.cpp
)

if(WIN32)
  list(APPEND CLOC_SEGFAULT_TEST_SOURCES
    ${CMAKE_CURRENT_SOURCE_DIR}/windows/safety_guard_caller_windows.cpp
    ${NEO_SOURCE_DIR}/offline_compiler/utilities/windows/safety_guard_windows.h
    ${NEO_SOURCE_DIR}/offline_compiler/utilities/windows/seh_exception.cpp
    ${NEO_SOURCE_DIR}/offline_compiler/utilities/windows/seh_exception.h
    ${NEO_SOURCE_DIR}/runtime/os_interface/windows/os_library.cpp
  )
else()
  list(APPEND CLOC_SEGFAULT_TEST_SOURCES
    ${CMAKE_CURRENT_SOURCE_DIR}/linux/safety_guard_caller_linux.cpp
    ${NEO_SOURCE_DIR}/runtime/os_interface/linux/os_library.cpp
  )
endif()

add_executable(ocloc_segfault_test ${CLOC_SEGFAULT_TEST_SOURCES})
target_link_libraries(ocloc_segfault_test gmock-gtest)
if(MSVC)
  target_compile_options(ocloc_segfault_test PRIVATE /Zi)
  set_property(TARGET ocloc_segfault_test APPEND PROPERTY LINK_FLAGS /DEBUG)
  target_link_libraries(ocloc_segfault_test dbghelp)
endif()
if(UNIX)
  target_link_libraries(ocloc_segfault_test dl pthread)
endif()

set(CLOC_SEGFAULT_TEST_INCLUDES
  ${NEO_SOURCE_DIR}
  ${THIRD_PARTY_DIR}
)

if(CMAKE_COMPILER_IS_GNUCC)
  target_compile_definitions(ocloc_segfault_test PRIVATE SKIP_SEGFAULT_TEST=1)
endif()

get_property(CLOC_FOLDER TARGET ocloc PROPERTY FOLDER)
set_property(TARGET ocloc_segfault_test PROPERTY FOLDER ${CLOC_FOLDER})

target_include_directories(ocloc_segfault_test BEFORE PRIVATE ${CLOC_SEGFAULT_TEST_INCLUDES})
create_project_source_tree(ocloc_segfault_test ${NEO_SOURCE_DIR}/offline_compiler ${NEO_SOURCE_DIR}/runtime ${NEO_SOURCE_DIR}/unit_tests)

add_custom_target(run_ocloc_segfault_test ALL
  DEPENDS ocloc_segfault_test
)

add_custom_command(
  TARGET run_ocloc_segfault_test
  POST_BUILD
  COMMAND echo Running ocloc_segfault_test
  COMMAND ocloc_segfault_test
  WORKING_DIRECTORY ${TargetDir}
)

set_property(TARGET run_ocloc_segfault_test PROPERTY FOLDER ${CLOC_FOLDER})
