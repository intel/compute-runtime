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

project(igdrcl_aub_tests)

include(${CMAKE_CURRENT_SOURCE_DIR}/common.cmake)

add_executable(igdrcl_aub_tests
  ${CMAKE_CURRENT_SOURCE_DIR}/aub_tests_configuration.cpp
  $<TARGET_OBJECTS:${BUILTINS_SOURCES_LIB_NAME}>
  $<TARGET_OBJECTS:${SCHEDULER_BINARY_LIB_NAME}>
  $<TARGET_OBJECTS:igdrcl_libult>
  $<TARGET_OBJECTS:igdrcl_libult_cs>
  $<TARGET_OBJECTS:igdrcl_libult_env>
)

set_target_properties(igdrcl_aub_tests PROPERTIES FOLDER ${TEST_PROJECTS_FOLDER})

add_custom_target(run_aub_tests)
set_target_properties(run_aub_tests PROPERTIES FOLDER ${TEST_PROJECTS_FOLDER})

target_include_directories(igdrcl_aub_tests PRIVATE
  ${GTEST_INCLUDE_DIR}
  ${GMOCK_INCLUDE_DIR}
)

if(WIN32)
  target_include_directories(igdrcl_aub_tests PRIVATE
    ${IGDRCL_SOURCE_DIR}/unit_tests/mocks${BRANCH_DIR_SUFFIX}
  )
endif()

target_sources(igdrcl_aub_tests PRIVATE
  ${IGDRCL_SOURCE_DIR}/runtime/gmm_helper/resource_info.cpp
)

if(WIN32)
  target_sources(igdrcl_aub_tests PRIVATE
    ${IGDRCL_SOURCE_DIR}/runtime/gmm_helper/gmm_memory.cpp
  )
endif()

add_subdirectories()

target_link_libraries(igdrcl_aub_tests igdrcl_mocks)
target_link_libraries(igdrcl_aub_tests ${NEO_MOCKABLE_LIB_NAME})
target_link_libraries(igdrcl_aub_tests igdrcl_mocks)
target_link_libraries(igdrcl_aub_tests gtest gmock ${IGDRCL_EXTRA_LIBS})

target_include_directories(igdrcl_aub_tests BEFORE PRIVATE ${IGDRCL_SOURCE_DIR}/unit_tests/gen_common)

foreach(GEN_TYPE ${ALL_GEN_TYPES})
  GEN_CONTAINS_PLATFORMS("TESTED" ${GEN_TYPE} GENX_HAS_PLATFORMS)
  if(${GENX_HAS_PLATFORMS})
    GET_PLATFORMS_FOR_GEN("TESTED" ${GEN_TYPE} TESTED_GENX_PLATFORMS)
    foreach(PLATFORM_IT ${TESTED_GENX_PLATFORMS})
      string(TOLOWER ${PLATFORM_IT} PLATFORM_IT_LOWER)
      add_custom_target(${PLATFORM_IT_LOWER}_aub_tests
        DEPENDS igdrcl_aub_tests
        DEPENDS copy_test_files_${PLATFORM_IT_LOWER}
      )
      add_dependencies(${PLATFORM_IT_LOWER}_aub_tests test_kernels_${PLATFORM_IT_LOWER})
      add_dependencies(${PLATFORM_IT_LOWER}_aub_tests test_kernel_${PLATFORM_IT_LOWER})
      PLATFORM_HAS_2_0(${GEN_TYPE} ${PLATFORM_IT} PLATFORM_SUPPORTS_2_0)
      if(${PLATFORM_SUPPORTS_2_0})
        add_dependencies(${PLATFORM_IT_LOWER}_aub_tests test_kernel_2_0_${PLATFORM_IT_LOWER})
      endif(${PLATFORM_SUPPORTS_2_0})
      add_custom_target(run_${PLATFORM_IT_LOWER}_aub_tests ALL DEPENDS ${PLATFORM_IT_LOWER}_aub_tests)
      add_dependencies(run_aub_tests run_${PLATFORM_IT_LOWER}_aub_tests)
      set_target_properties(${PLATFORM_IT_LOWER}_aub_tests PROPERTIES FOLDER "${PLATFORM_SPECIFIC_TARGETS_FOLDER}/${PLATFORM_IT_LOWER}")
      set_target_properties(run_${PLATFORM_IT_LOWER}_aub_tests PROPERTIES FOLDER "${PLATFORM_SPECIFIC_TARGETS_FOLDER}/${PLATFORM_IT_LOWER}")
      if(WIN32)
        add_dependencies(${PLATFORM_IT_LOWER}_aub_tests mock_gdi)
      endif()
      GET_TEST_CONFIGURATIONS_FOR_PLATFORM("AUB_TESTS" ${GEN_TYPE} ${PLATFORM_IT} PLATFORM_CONFIGURATIONS)
      foreach(PLATFORM_CONFIGURATION ${PLATFORM_CONFIGURATIONS})
        string(REPLACE "/" ";" CONFIGURATION_PARAMS ${PLATFORM_CONFIGURATION})
        list(GET CONFIGURATION_PARAMS 1 SLICES)
        list(GET CONFIGURATION_PARAMS 2 SUBSLICES)
        list(GET CONFIGURATION_PARAMS 3 EU_PER_SS)
        neo_run_aub_target(${PLATFORM_IT_LOWER} "${PLATFORM_IT} ${SLICES}x${SUBSLICES}x${EU_PER_SS}" ${PLATFORM_IT_LOWER} ${SLICES} ${SUBSLICES} ${EU_PER_SS})
      endforeach()
    endforeach()
  endif()
endforeach()
create_project_source_tree(igdrcl_aub_tests ${IGDRCL_SOURCE_DIR}/runtime ${IGDRCL_SOURCE_DIR}/unit_tests)
