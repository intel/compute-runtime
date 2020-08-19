#
# Copyright (C) 2020 Intel Corporation
#
# SPDX-License-Identifier: MIT
#

set(NEO_CORE_OS_INTERFACE_TESTS_LINUX
    ${CMAKE_CURRENT_SOURCE_DIR}/CMakeLists.txt
    ${CMAKE_CURRENT_SOURCE_DIR}${BRANCH_DIR_SUFFIX}/drm_query_tests.cpp
)

set_property(GLOBAL PROPERTY NEO_CORE_OS_INTERFACE_TESTS_LINUX ${NEO_CORE_OS_INTERFACE_TESTS_LINUX})

if(UNIX)
  target_sources(${TARGET_NAME} PRIVATE
                 ${NEO_CORE_OS_INTERFACE_TESTS_LINUX}
  )
endif()
