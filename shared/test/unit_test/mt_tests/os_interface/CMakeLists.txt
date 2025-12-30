#
# Copyright (C) 2018-2026 Intel Corporation
#
# SPDX-License-Identifier: MIT
#

if(UNIX)
  target_sources(neo_shared_mt_tests PRIVATE
                 ${CMAKE_CURRENT_SOURCE_DIR}/linux/drm_gem_close_worker_mt_tests.cpp
                 ${CMAKE_CURRENT_SOURCE_DIR}/linux/drm_memory_manager_mt_tests.cpp
                 ${CMAKE_CURRENT_SOURCE_DIR}/linux/ioctl_helper_xe_mt_tests.cpp
                 ${CMAKE_CURRENT_SOURCE_DIR}/linux/ipc_socket_mt_tests.cpp
  )
else()
  target_sources(neo_shared_mt_tests PRIVATE
                 ${CMAKE_CURRENT_SOURCE_DIR}/windows/os_thread_win_tests.cpp
                 ${CMAKE_CURRENT_SOURCE_DIR}/windows/residency_controller_tests.cpp
  )
endif()
