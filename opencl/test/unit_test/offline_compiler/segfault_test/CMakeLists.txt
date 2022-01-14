#
# Copyright (C) 2018-2022 Intel Corporation
#
# SPDX-License-Identifier: MIT
#

set(CLOC_SEGFAULT_TEST_SOURCES
    ${CMAKE_CURRENT_SOURCE_DIR}/CMakeLists.txt
    ${CMAKE_CURRENT_SOURCE_DIR}/segfault_helper.h
    ${CMAKE_CURRENT_SOURCE_DIR}/main.cpp
    ${NEO_SHARED_DIRECTORY}/helpers/abort.cpp
    ${NEO_SHARED_DIRECTORY}/os_interface/os_library.h
    ${NEO_SHARED_TEST_DIRECTORY}/common/libult/signal_utils.h
    ${NEO_SHARED_TEST_DIRECTORY}/unit_test/helpers/debug_helpers.cpp
    ${NEO_SOURCE_DIR}/opencl/test/unit_test/offline_compiler/ocloc_tests_configuration.cpp
)

if(WIN32)
  list(APPEND CLOC_SEGFAULT_TEST_SOURCES
       ${CMAKE_CURRENT_SOURCE_DIR}/windows/safety_guard_caller_windows.cpp
       ${NEO_SHARED_DIRECTORY}/os_interface/windows/os_library_win.cpp
       ${NEO_SHARED_DIRECTORY}/os_interface/windows/os_library_win.h
       ${NEO_SHARED_TEST_DIRECTORY}/common/os_interface/windows/signal_utils.cpp
       ${OCLOC_DIRECTORY}/source/utilities/windows/safety_guard_windows.h
       ${OCLOC_DIRECTORY}/source/utilities/windows/seh_exception.cpp
       ${OCLOC_DIRECTORY}/source/utilities/windows/seh_exception.h
  )
else()
  list(APPEND CLOC_SEGFAULT_TEST_SOURCES
       ${CMAKE_CURRENT_SOURCE_DIR}/linux/safety_guard_caller_linux.cpp
       ${NEO_SHARED_DIRECTORY}/os_interface/linux/os_library_linux.cpp
       ${NEO_SHARED_DIRECTORY}/os_interface/linux/os_library_linux.h
       ${NEO_SHARED_DIRECTORY}/os_interface/linux/sys_calls_linux.cpp
       ${NEO_SHARED_TEST_DIRECTORY}/common/os_interface/linux/signal_utils.cpp
       ${OCLOC_DIRECTORY}/source/linux/os_library_ocloc_helper.cpp
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
create_project_source_tree(ocloc_segfault_test)

add_custom_target(run_ocloc_segfault_test ALL
                  DEPENDS ocloc_segfault_test
)

if(NOT USE_TSAN)
  add_custom_command(
                     TARGET run_ocloc_segfault_test
                     POST_BUILD
                     COMMAND echo Running ocloc_segfault_test
                     COMMAND ocloc_segfault_test
                     WORKING_DIRECTORY ${TargetDir}
  )
endif()

set_property(TARGET run_ocloc_segfault_test PROPERTY FOLDER ${CLOC_FOLDER})
