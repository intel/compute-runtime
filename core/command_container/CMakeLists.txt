#
# Copyright (C) 2019 Intel Corporation
#
# SPDX-License-Identifier: MIT
#

set(NEO_CORE_COMMAND_CONTAINER
  ${CMAKE_CURRENT_SOURCE_DIR}/CMakeLists.txt
  ${CMAKE_CURRENT_SOURCE_DIR}/cmdcontainer.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/cmdcontainer.h
)

set_property(GLOBAL PROPERTY NEO_CORE_COMMAND_CONTAINER ${NEO_CORE_COMMAND_CONTAINER})
