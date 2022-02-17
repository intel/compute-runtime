#
# Copyright (C) 2021-2022 Intel Corporation
#
# SPDX-License-Identifier: MIT
#

if(TESTS_ADLP)
  set(IGDRCL_SRCS_tests_gen12lp_adlp_excludes
      ${CMAKE_CURRENT_SOURCE_DIR}/excludes_ocl_adlp.cpp
  )
  set_property(GLOBAL APPEND PROPERTY IGDRCL_SRCS_tests_excludes ${IGDRCL_SRCS_tests_gen12lp_adlp_excludes})

  set(IGDRCL_SRCS_tests_gen12lp_adlp
      ${IGDRCL_SRCS_tests_gen12lp_adlp_excludes}
      ${CMAKE_CURRENT_SOURCE_DIR}/CMakeLists.txt
      ${CMAKE_CURRENT_SOURCE_DIR}/test_device_caps_adlp.cpp
  )

  get_property(NEO_CORE_TESTS_GEN12LP_ADLP GLOBAL PROPERTY NEO_CORE_TESTS_GEN12LP_ADLP)
  list(APPEND IGDRCL_SRCS_tests_gen12lp_adlp ${NEO_CORE_TESTS_GEN12LP_ADLP})

  target_sources(igdrcl_tests PRIVATE ${IGDRCL_SRCS_tests_gen12lp_adlp})
  add_subdirectories()
  neo_copy_test_files_with_revision(copy_test_files_adlp_0 adlp 0)
  add_dependencies(copy_test_files_per_product copy_test_files_adlp_0)
endif()
