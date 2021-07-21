#
# Copyright (C) 2021 Intel Corporation
#
# SPDX-License-Identifier: MIT
#

set(NEO_CORE_AIL_TESTS_LINUX
    ${CMAKE_CURRENT_SOURCE_DIR}/CMakeLists.txt
    ${CMAKE_CURRENT_SOURCE_DIR}/ail_linux_tests.cpp
)

set_property(GLOBAL PROPERTY NEO_CORE_AIL_TESTS_LINUX ${NEO_CORE_AIL_TESTS_LINUX})

if(UNIX)
  target_sources(${TARGET_NAME} PRIVATE
                 ${NEO_CORE_AIL_TESTS_LINUX}
  )
endif()
add_subdirectories()
