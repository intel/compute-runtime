#
# Copyright (C) 2022 Intel Corporation
#
# SPDX-License-Identifier: MIT
#

if(UNIX)
  target_sources(neo_shared_tests PRIVATE
                 ${CMAKE_CURRENT_SOURCE_DIR}/hw_info_config_tests_dg1.cpp
  )
endif()
