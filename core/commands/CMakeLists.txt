#
# Copyright (C) 2018-2019 Intel Corporation
#
# SPDX-License-Identifier: MIT
#

set(NEO_CORE_COMMANDS
  ${CMAKE_CURRENT_SOURCE_DIR}/CMakeLists.txt
  ${CMAKE_CURRENT_SOURCE_DIR}/bxml_generator_glue.h
)

set_property(GLOBAL PROPERTY NEO_CORE_COMMANDS ${NEO_CORE_COMMANDS})
