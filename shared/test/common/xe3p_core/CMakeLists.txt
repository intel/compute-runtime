#
# Copyright (C) 2025 Intel Corporation
#
# SPDX-License-Identifier: MIT
#

if(TESTS_XE3P_CORE)
  target_sources(neo_libult_common PRIVATE
                 ${CMAKE_CURRENT_SOURCE_DIR}/CMakeLists.txt
                 ${CMAKE_CURRENT_SOURCE_DIR}/hw_parse_xe3p.inl
                 ${CMAKE_CURRENT_SOURCE_DIR}/test_traits_xe3p_core.h
  )
endif()
