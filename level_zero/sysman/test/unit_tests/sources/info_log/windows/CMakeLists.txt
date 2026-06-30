#
# Copyright (C) 2026 Intel Corporation
#
# SPDX-License-Identifier: MIT
#

if(WIN32)
  target_sources(${TARGET_NAME}
                 PRIVATE
                 ${CMAKE_CURRENT_SOURCE_DIR}/CMakeLists.txt
                 ${CMAKE_CURRENT_SOURCE_DIR}/test_zes_sysman_info_log.cpp
  )
endif()