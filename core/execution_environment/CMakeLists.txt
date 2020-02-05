#
# Copyright (C) 2018-2020 Intel Corporation
#
# SPDX-License-Identifier: MIT
#

set(NEO_CORE_EXECUTION_ENVIRONMENT
  ${CMAKE_CURRENT_SOURCE_DIR}/CMakeLists.txt
  ${CMAKE_CURRENT_SOURCE_DIR}/execution_environment.h
  ${CMAKE_CURRENT_SOURCE_DIR}/execution_environment.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/root_device_environment.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/root_device_environment.h

)

set_property(GLOBAL PROPERTY NEO_CORE_EXECUTION_ENVIRONMENT ${NEO_CORE_EXECUTION_ENVIRONMENT})
