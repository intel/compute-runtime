#
# Copyright (C) 2021-2025 Intel Corporation
#
# SPDX-License-Identifier: MIT
#

if(UNIX)
  target_sources(neo_shared_tests PRIVATE
                 ${CMAKE_CURRENT_SOURCE_DIR}/CMakeLists.txt
                 ${CMAKE_CURRENT_SOURCE_DIR}/ail_linux_tests.cpp
                 ${CMAKE_CURRENT_SOURCE_DIR}/ail_linux_tests_ov_comp_wa.cpp
  )
endif()