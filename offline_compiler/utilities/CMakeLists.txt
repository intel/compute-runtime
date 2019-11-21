#
# Copyright (C) 2018-2019 Intel Corporation
#
# SPDX-License-Identifier: MIT
#

set(CLOC_SRCS_UTILITIES
  ${CMAKE_CURRENT_SOURCE_DIR}/safety_caller.h
  ${CMAKE_CURRENT_SOURCE_DIR}/get_current_dir.h
)

if(WIN32)
  list(APPEND CLOC_SRCS_UTILITIES
    ${CMAKE_CURRENT_SOURCE_DIR}/windows/safety_caller_windows.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/windows/safety_guard_windows.h
    ${CMAKE_CURRENT_SOURCE_DIR}/windows/seh_exception.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/windows/seh_exception.h
    ${CMAKE_CURRENT_SOURCE_DIR}/windows/get_current_dir_windows.cpp
  )
else()
  list(APPEND CLOC_SRCS_UTILITIES
    ${CMAKE_CURRENT_SOURCE_DIR}/linux/safety_caller_linux.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/linux/get_current_dir_linux.cpp
  )
endif()

target_sources(ocloc PRIVATE ${CLOC_SRCS_UTILITIES})
