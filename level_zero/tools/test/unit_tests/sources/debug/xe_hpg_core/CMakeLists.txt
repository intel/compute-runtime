#
# Copyright (C) 2024 Intel Corporation
#
# SPDX-License-Identifier: MIT
#

if(TESTS_XE_HPG_CORE)
  target_sources(${TARGET_NAME} PRIVATE
                 ${CMAKE_CURRENT_SOURCE_DIR}/CMakeLists.txt
                 ${CMAKE_CURRENT_SOURCE_DIR}/debug_session_tests_xe_hpg_core.cpp
  )
endif()
