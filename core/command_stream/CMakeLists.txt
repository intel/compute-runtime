#
# Copyright (C) 2019 Intel Corporation
#
# SPDX-License-Identifier: MIT
#

set(NEO_CORE_COMMAND_STREAM
  ${CMAKE_CURRENT_SOURCE_DIR}/CMakeLists.txt
  ${CMAKE_CURRENT_SOURCE_DIR}/preemption_mode.h
)

set_property(GLOBAL PROPERTY NEO_CORE_COMMAND_STREAM ${NEO_CORE_COMMAND_STREAM})
