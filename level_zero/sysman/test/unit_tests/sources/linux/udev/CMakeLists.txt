#
# Copyright (C) 2026 Intel Corporation
#
# SPDX-License-Identifier: MIT
#

if(UNIX)
  if(LIBUDEV_FOUND)
    target_sources(${TARGET_NAME} PRIVATE
                   ${CMAKE_CURRENT_SOURCE_DIR}/CMakeLists.txt
                   ${CMAKE_CURRENT_SOURCE_DIR}/mock_udev_os_library.cpp
                   ${CMAKE_CURRENT_SOURCE_DIR}/mock_udev_os_library.h
                   ${CMAKE_CURRENT_SOURCE_DIR}/test_sysman_udev_lib.cpp
    )
  endif()
endif()
