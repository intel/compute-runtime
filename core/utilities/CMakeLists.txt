#
# Copyright (C) 2019 Intel Corporation
#
# SPDX-License-Identifier: MIT
#

set(NEO_CORE_UTILITIES
  ${CMAKE_CURRENT_SOURCE_DIR}/CMakeLists.txt
  ${CMAKE_CURRENT_SOURCE_DIR}/spinlock.h
)

set_property(GLOBAL PROPERTY NEO_CORE_UTILITIES ${NEO_CORE_UTILITIES})
