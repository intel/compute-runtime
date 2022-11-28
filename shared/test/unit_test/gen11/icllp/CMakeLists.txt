#
# Copyright (C) 2022 Intel Corporation
#
# SPDX-License-Identifier: MIT
#

if(TESTS_ICLLP)
  set(IGDRCL_SRCS_tests_gen11_icllp_excludes ${CMAKE_CURRENT_SOURCE_DIR}/excludes_gen11_icllp.cpp)
  set_property(GLOBAL APPEND PROPERTY IGDRCL_SRCS_tests_excludes ${IGDRCL_SRCS_tests_gen11_icllp_excludes})

  set(NEO_SHARED_tests_gen11_icllp ${IGDRCL_SRCS_tests_gen11_icllp_excludes})

  target_sources(neo_shared_tests PRIVATE
                 ${NEO_SHARED_tests_gen11_icllp}
                 ${CMAKE_CURRENT_SOURCE_DIR}/test_hw_info_config_icllp.cpp
                 ${CMAKE_CURRENT_SOURCE_DIR}/test_program_media_sampler_icllp.cpp
  )

  add_subdirectories()
endif()
