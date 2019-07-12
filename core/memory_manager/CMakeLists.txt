#
# Copyright (C) 2019 Intel Corporation
#
# SPDX-License-Identifier: MIT
#

set(NEO_CORE_MEMORY_MANAGER
  ${CMAKE_CURRENT_SOURCE_DIR}/CMakeLists.txt
  ${CMAKE_CURRENT_SOURCE_DIR}/eviction_status.h
  ${CMAKE_CURRENT_SOURCE_DIR}/residency_handler.h
)

set_property(GLOBAL PROPERTY NEO_CORE_MEMORY_MANAGER ${NEO_CORE_MEMORY_MANAGER})
