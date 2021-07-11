#
# Copyright (C) 2021 Intel Corporation
#
# SPDX-License-Identifier: MIT
#

set(NEO_CORE_EXECUTION_ENVIRONMENT_WINDOWS
    ${CMAKE_CURRENT_SOURCE_DIR}/CMakeLists.txt
    ${CMAKE_CURRENT_SOURCE_DIR}/execution_environment_windows.cpp
)

if(WIN32)
  set_property(GLOBAL PROPERTY NEO_CORE_EXECUTION_ENVIRONMENT_WINDOWS ${NEO_CORE_EXECUTION_ENVIRONMENT_WINDOWS})
endif()
