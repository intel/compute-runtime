# Copyright (c) 2018, Intel Corporation
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

project(cloc_tests)

set(IGDRCL_SRCS_cloc
  ${IGDRCL_SOURCE_DIR}/offline_compiler/offline_compiler.cpp
)

link_directories(${GMM_LIB_PATHS})

set(IGDRCL_SRCS_offline_compiler_mock
  mock/mock_offline_compiler.h
)

set(IGDRCL_SRCS_offline_compiler_tests
  ${CMAKE_CURRENT_SOURCE_DIR}/CMakeLists.txt
  ${CMAKE_CURRENT_SOURCE_DIR}/environment.h
  ${CMAKE_CURRENT_SOURCE_DIR}/main.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/offline_compiler_tests.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/offline_compiler_tests.h
  ${IGDRCL_SOURCE_DIR}/runtime/helpers/abort.cpp
  ${IGDRCL_SOURCE_DIR}/runtime/helpers/file_io.cpp
  ${IGDRCL_SOURCE_DIR}/runtime/memory_manager/deferred_deleter.cpp
  ${IGDRCL_SOURCE_DIR}/runtime/memory_manager/deferred_deleter.h
  ${IGDRCL_SOURCE_DIR}/unit_tests/helpers/test_files.cpp
  ${IGDRCL_SRCS_cloc}
  ${IGDRCL_SRCS_offline_compiler_mock}
  ${IGDRCL_SRCS_offline_compiler_tests_linux}
  ${IGDRCL_SRCS_offline_compiler_tests_windows}
  ${CLOC_SRCS_LIB}
)

link_directories(${CMAKE_RUNTIME_OUTPUT_DIRECTORY})

add_executable(cloc_tests ${IGDRCL_SRCS_offline_compiler_tests})

target_include_directories(cloc_tests PRIVATE
  ${CLOC_INCLUDES}
  ${GTEST_INCLUDE_DIR}
  ${GMOCK_INCLUDE_DIR}
  ${GMM_INCLUDE_PATHS}
)
target_compile_definitions(cloc_tests PUBLIC MOCKABLE_VIRTUAL=virtual ${SUPPORTED_GEN_FLAGS_DEFINITONS} DEFAULT_PLATFORM=${DEFAULT_SUPPORTED_PLATFORM})

if(IGC_OCL_ADAPTOR_DIR) # IGC/AdaptorOCL
  target_include_directories(cloc_tests PRIVATE "${IGC_OCL_ADAPTOR_DIR}")
endif(IGC_OCL_ADAPTOR_DIR)

if(CIF_BASE_DIR)
  target_include_directories(cloc_tests PRIVATE "${CIF_BASE_DIR}")
endif(CIF_BASE_DIR)

add_definitions(${CLOC_LIB_FLAGS_DEFINITIONS})

target_link_libraries(cloc_tests igdrcl_mocks gtest elflib)

if(UNIX)
  target_link_libraries(cloc_tests dl pthread)
  target_include_directories(cloc_tests PRIVATE
    ${GMM_SOURCE_DIR}/inc
    ${GMM_SOURCE_DIR}/inc/common
  )
endif()
if(WIN32)
  target_link_libraries(cloc_tests)
  target_include_directories(cloc_tests PRIVATE ${UMKM_SHAREDDATA_INCLUDE_PATHS})
endif()

create_project_source_tree(cloc_tests ${IGDRCL_SOURCE_DIR}/runtime ${IGDRCL_SOURCE_DIR}/unit_tests ${IGDRCL_SOURCE_DIR})

set_target_properties(cloc_tests PROPERTIES FOLDER "offline_compiler")
set_property(TARGET cloc_tests APPEND_STRING PROPERTY COMPILE_FLAGS ${ASAN_FLAGS})

string(TOLOWER ${DEFAULT_TESTED_PLATFORM} CLOC_DEFAULT_DEVICE)

add_custom_target(run_cloc_tests ALL
  DEPENDS cloc_tests
  DEPENDS copy_test_files_${CLOC_DEFAULT_DEVICE}
  DEPENDS test_kernels_${CLOC_DEFAULT_DEVICE}
)
set_target_properties(run_cloc_tests PROPERTIES FOLDER "offline_compiler")

if(MSVC)
  #####################
  # MSVC / Windows
  #####################
  set( TargetDir $<TARGET_FILE_DIR:cloc_tests> )

  add_custom_command(
    TARGET run_cloc_tests
    POST_BUILD
    COMMAND echo deleting offline compiler files and directories...
    COMMAND ${CMAKE_COMMAND} -E remove ${TargetDir}/${CLOC_DEFAULT_DEVICE}/copybuffer_${CLOC_DEFAULT_DEVICE}.bc
    COMMAND ${CMAKE_COMMAND} -E remove ${TargetDir}/${CLOC_DEFAULT_DEVICE}/copybuffer_${CLOC_DEFAULT_DEVICE}.gen
    COMMAND ${CMAKE_COMMAND} -E remove ${TargetDir}/${CLOC_DEFAULT_DEVICE}/copybuffer_${CLOC_DEFAULT_DEVICE}.bin
    COMMAND ${CMAKE_COMMAND} -E remove_directory ${TargetDir}/${CLOC_DEFAULT_DEVICE}/offline_compiler_test
    COMMAND echo cd ${TargetDir}/${CLOC_DEFAULT_DEVICE}
    COMMAND cd ${TargetDir}/${CLOC_DEFAULT_DEVICE}
    COMMAND ../cloc_tests --device ${CLOC_DEFAULT_DEVICE}
  )
else()
  #####################
  # Linux
  #####################
  set( TargetDir ${CMAKE_RUNTIME_OUTPUT_DIRECTORY} )
  add_custom_command(
    TARGET run_cloc_tests
    POST_BUILD
    COMMAND echo deleting offline compiler files and directories...
    COMMAND ${CMAKE_COMMAND} -E remove ${TargetDir}/${CLOC_DEFAULT_DEVICE}/*.bc
    COMMAND ${CMAKE_COMMAND} -E remove ${TargetDir}/${CLOC_DEFAULT_DEVICE}/*.gen
    COMMAND ${CMAKE_COMMAND} -E remove ${TargetDir}/${CLOC_DEFAULT_DEVICE}/*.ll
    COMMAND ${CMAKE_COMMAND} -E remove ${TargetDir}/${CLOC_DEFAULT_DEVICE}/*.bin
    COMMAND ${CMAKE_COMMAND} -E remove_directory "${TargetDir}/offline_compiler_test"
    COMMAND cloc_tests --device ${CLOC_DEFAULT_DEVICE}
    WORKING_DIRECTORY ${TargetDir}/${CLOC_DEFAULT_DEVICE}
  )
endif()
