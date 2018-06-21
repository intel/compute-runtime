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

if(NOT USE_STATIC_GMM)
  set(target_name mock_gmm)
  project(${target_name})

  # Setting up our local list of test files
  set(IGDRCL_SRCS_tests_mock_gmm
    ${CMAKE_CURRENT_SOURCE_DIR}/CMakeLists.txt
    ${CMAKE_CURRENT_SOURCE_DIR}/mock_gmm.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/mock_gmm.h
    ${CMAKE_CURRENT_SOURCE_DIR}${BRANCH_DIR_SUFFIX}/init_platform.cpp
  )
  add_library(${target_name} EXCLUDE_FROM_ALL SHARED ${IGDRCL_SRCS_tests_mock_gmm})

  target_include_directories(${target_name} PRIVATE
    ${WDK_INCLUDE_PATHS}
    ${UMKM_SHAREDDATA_INCLUDE_PATHS}
  )

  set_target_properties(
    ${target_name}
    PROPERTIES DEBUG_OUTPUT_NAME ${target_name}
    RELEASE_OUTPUT_NAME ${target_name}
    RELEASEINTERNAL_OUTPUT_NAME ${target_name}
    OUTPUT_NAME ${target_name}
  )

  create_project_source_tree(${target_name})
  set_target_properties(${target_name} PROPERTIES FOLDER "test mocks")
  target_compile_definitions(${target_name} PUBLIC GMM_LIB_DLL GMM_LIB_DLL_EXPORTS)
  if(NOT USE_STATIC_GMM)
    add_dependencies(unit_tests ${target_name})
  endif()
endif()
