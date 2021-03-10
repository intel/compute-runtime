#
# Copyright (C) 2020-2021 Intel Corporation
#
# SPDX-License-Identifier: MIT
#

if(UNIX)
  target_sources(${TARGET_NAME}
                 PRIVATE
                 ${CMAKE_CURRENT_SOURCE_DIR}/CMakeLists.txt
                 ${CMAKE_CURRENT_SOURCE_DIR}/mock_zes_sysman_diagnostics.h
                 ${CMAKE_CURRENT_SOURCE_DIR}/test_zes_sysman_diagnostics.cpp
  )
endif()
