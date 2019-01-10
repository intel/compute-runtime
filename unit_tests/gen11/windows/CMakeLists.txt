#
# Copyright (C) 2018-2019 Intel Corporation
#
# SPDX-License-Identifier: MIT
#

set(IGDRCL_SRCS_tests_gen11_windows
  ${CMAKE_CURRENT_SOURCE_DIR}/CMakeLists.txt
  ${CMAKE_CURRENT_SOURCE_DIR}/gmm_callbacks_tests_gen11.cpp
)
if(WIN32)
  target_sources(igdrcl_tests PRIVATE ${IGDRCL_SRCS_tests_gen11_windows})
endif()
