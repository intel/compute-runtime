#
# Copyright (C) 2019 Intel Corporation
#
# SPDX-License-Identifier: MIT
#

set(NEO_CORE_OS_INTERFACE_LINUX
  ${CMAKE_CURRENT_SOURCE_DIR}/CMakeLists.txt
  ${CMAKE_CURRENT_SOURCE_DIR}/debug_env_reader.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/debug_env_reader.h
)

set_property(GLOBAL PROPERTY NEO_CORE_OS_INTERFACE_LINUX ${NEO_CORE_OS_INTERFACE_LINUX})
