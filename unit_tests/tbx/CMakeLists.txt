# Copyright (c) 2017 - 2018, Intel Corporation
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

cmake_minimum_required(VERSION 3.2.0 FATAL_ERROR)

project(igdrcl_tbx_tests)

add_executable(igdrcl_tbx_tests 
    "${CMAKE_CURRENT_SOURCE_DIR}/main_tbx.cpp"
    "${IGDRCL_SOURCE_DIR}/runtime/dll/create_command_stream.cpp"
    "${IGDRCL_SOURCE_DIR}/unit_tests/abort.cpp"
    "${IGDRCL_SOURCE_DIR}/unit_tests/options.cpp"
    "${IGDRCL_SOURCE_DIR}/unit_tests/program/evaluate_unhandled_token_ult.cpp"
    $<TARGET_OBJECTS:igdrcl_libult>
)

target_link_libraries(igdrcl_tbx_tests ${NEO_MOCKABLE_LIB_NAME} igdrcl_mocks gtest gmock igdrcl_mocks)

target_compile_definitions(igdrcl_tbx_tests PRIVATE MOCKABLE_VIRTUAL=virtual)
if(UNIX)
  target_link_libraries(igdrcl_tbx_tests dl pthread)
elseif(WIN32)
    target_include_directories(igdrcl_tbx_tests PRIVATE
      ${IGDRCL_SOURCE_DIR}/unit_tests/mocks${BRANCH_DIR_SUFFIX}
    )
	target_sources(igdrcl_tbx_tests PUBLIC
		${IGDRCL_SOURCE_DIR}/unit_tests/os_interface/windows/wddm_create.cpp
		${IGDRCL_SOURCE_DIR}/unit_tests/os_interface/windows/ult_dxgi_factory.cpp
	 )
	target_link_libraries(igdrcl_tbx_tests Ws2_32)
endif(UNIX)

target_include_directories(igdrcl_tbx_tests PRIVATE
	${GTEST_INCLUDE_DIR}
	${GMOCK_INCLUDE_DIR}
	${UMKM_SHAREDDATA_INCLUDE_PATHS}
	${GMM_INCLUDE_PATHS}
	${KHRONOS_HEADERS_DIR}
	${IGDRCL__IGC_INCLUDE_DIR}
)

add_custom_target(run_tbx_tests ALL DEPENDS igdrcl_tbx_tests)
add_custom_command(
	TARGET run_tbx_tests
	POST_BUILD
	COMMAND WORKING_DIRECTORY ${TargetDir}
	COMMAND echo Target Directory is: ${TargetDir}
	COMMAND echo Running TBX tests
	COMMAND igdrcl_tbx_tests
)
add_dependencies(run_unit_tests run_tbx_tests)

set_target_properties(igdrcl_tbx_tests PROPERTIES FOLDER "opencl runtime")
set_target_properties(run_tbx_tests PROPERTIES FOLDER "opencl runtime")
