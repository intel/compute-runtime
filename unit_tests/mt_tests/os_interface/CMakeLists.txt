#
# Copyright (C) 2018 Intel Corporation
#
# SPDX-License-Identifier: MIT
#

if(UNIX)
  target_sources(igdrcl_mt_tests PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/linux/drm_memory_manager_mt_tests.cpp
  )
endif()
