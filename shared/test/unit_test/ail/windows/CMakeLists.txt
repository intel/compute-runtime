#
# Copyright (C) 2021 Intel Corporation
#
# SPDX-License-Identifier: MIT
#

set(NEO_CORE_AIL_TESTS_WINDOWS
    ${CMAKE_CURRENT_SOURCE_DIR}/CMakeLists.txt
    ${CMAKE_CURRENT_SOURCE_DIR}/ail_windows_tests.cpp
)

set_property(GLOBAL PROPERTY NEO_CORE_AIL_TESTS_WINDOWS ${NEO_CORE_AIL_TESTS_WINDOWS})

if(WIN32)
  target_sources(${TARGET_NAME} PRIVATE
                 ${NEO_CORE_AIL_TESTS_WINDOWS}
  )
endif()
