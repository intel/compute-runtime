#
# Copyright (C) 2019-2022 Intel Corporation
#
# SPDX-License-Identifier: MIT
#

if(TESTS_ICLLP)
  set(IGDRCL_SRCS_tests_gen11_icllp_excludes
      ${CMAKE_CURRENT_SOURCE_DIR}/excludes_ocl_icllp.cpp
  )
  set_property(GLOBAL APPEND PROPERTY IGDRCL_SRCS_tests_excludes ${IGDRCL_SRCS_tests_gen11_icllp_excludes})

  set(IGDRCL_SRCS_tests_gen11_icllp
      ${IGDRCL_SRCS_tests_gen11_icllp_excludes}
      ${CMAKE_CURRENT_SOURCE_DIR}/CMakeLists.txt
      ${CMAKE_CURRENT_SOURCE_DIR}/test_hw_info_config_icllp.cpp
      ${CMAKE_CURRENT_SOURCE_DIR}/test_device_caps_icllp.cpp
      ${CMAKE_CURRENT_SOURCE_DIR}/test_media_kernel_icllp.cpp
      ${CMAKE_CURRENT_SOURCE_DIR}/test_program_media_sampler_icllp.cpp
  )
  target_sources(igdrcl_tests PRIVATE ${IGDRCL_SRCS_tests_gen11_icllp})
  add_subdirectories()
  neo_copy_test_files_with_revision(copy_test_files_icllp_0 icllp 0)
  add_dependencies(copy_test_files_per_product copy_test_files_icllp_0)
endif()
