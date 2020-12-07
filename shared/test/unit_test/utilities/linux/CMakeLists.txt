#
# Copyright (C) 2021 Intel Corporation
#
# SPDX-License-Identifier: MIT
#

set(UTILITIES_CPUINFO_TESTS_LINUX
    ${CMAKE_CURRENT_SOURCE_DIR}/CMakeLists.txt
    ${CMAKE_CURRENT_SOURCE_DIR}/cpuinfo_tests_linux.cpp
)

if(UNIX)
  target_sources(${TARGET_NAME} PRIVATE
                 ${UTILITIES_CPUINFO_TESTS_LINUX}
  )
endif()
