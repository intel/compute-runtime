#
# Copyright (C) 2018-2025 Intel Corporation
#
# SPDX-License-Identifier: MIT
#

if(UNIX)
  target_sources(igdrcl_mt_tests PRIVATE
                 ${CMAKE_CURRENT_SOURCE_DIR}/linux/drm_gem_close_worker_mt_tests.cpp
                 ${CMAKE_CURRENT_SOURCE_DIR}/linux/drm_memory_manager_mt_tests.cpp
                 ${CMAKE_CURRENT_SOURCE_DIR}/linux/ioctl_helper_xe_mt_tests.cpp
  )
endif()
