#
# Copyright (C) 2017-2018 Intel Corporation
#
# SPDX-License-Identifier: MIT
#

set(IGDRCL_SRCS_tests_gen10_windows
  ${CMAKE_CURRENT_SOURCE_DIR}/CMakeLists.txt
  ${CMAKE_CURRENT_SOURCE_DIR}/gmm_callbacks_tests_gen10.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/hw_info_config_tests_gen10.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/os_interface_tests_gen10.cpp
)
if(WIN32)
  target_sources(igdrcl_tests PRIVATE ${IGDRCL_SRCS_tests_gen10_windows})
endif()
