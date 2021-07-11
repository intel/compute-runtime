#
# Copyright (C) 2021 Intel Corporation
#
# SPDX-License-Identifier: MIT
#

set(NEO_CORE_EXECUTION_ENVIRONMENT_LINUX
    ${CMAKE_CURRENT_SOURCE_DIR}/CMakeLists.txt
    ${CMAKE_CURRENT_SOURCE_DIR}/execution_environment_linux.cpp
)

if(UNIX)
  set_property(GLOBAL PROPERTY NEO_CORE_EXECUTION_ENVIRONMENT_LINUX ${NEO_CORE_EXECUTION_ENVIRONMENT_LINUX})
endif()
