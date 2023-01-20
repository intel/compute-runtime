#
# Copyright (C) 2019-2022 Intel Corporation
#
# SPDX-License-Identifier: MIT
#

if(UNIX)
  target_sources(neo_shared_tests PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/kmd_notify_linux_tests.cpp)

  add_subdirectories()
endif()
