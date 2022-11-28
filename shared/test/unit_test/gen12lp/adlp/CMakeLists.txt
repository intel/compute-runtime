#
# Copyright (C) 2021-2022 Intel Corporation
#
# SPDX-License-Identifier: MIT
#

if(TESTS_ADLP)
  set(IGDRCL_SRCS_tests_gen12lp_adlp_excludes ${CMAKE_CURRENT_SOURCE_DIR}/excludes_adlp.cpp)
  set_property(GLOBAL APPEND PROPERTY IGDRCL_SRCS_tests_excludes ${IGDRCL_SRCS_tests_gen12lp_adlp_excludes})

  target_sources(neo_shared_tests PRIVATE
                 ${IGDRCL_SRCS_tests_gen12lp_adlp_excludes}
                 ${CMAKE_CURRENT_SOURCE_DIR}/CMakeLists.txt
                 ${CMAKE_CURRENT_SOURCE_DIR}/command_stream_receiver_hw_tests_adlp.cpp
                 ${CMAKE_CURRENT_SOURCE_DIR}/preamble_helper_tests_adlp.cpp
                 ${CMAKE_CURRENT_SOURCE_DIR}/test_device_caps_adlp.cpp
                 ${CMAKE_CURRENT_SOURCE_DIR}/test_hw_info_config_adlp.cpp
  )

  add_subdirectories()
endif()
