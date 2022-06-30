#
# Copyright (C) 2021 Intel Corporation
#
# SPDX-License-Identifier: MIT
#

if(${NEO_TARGET_PROCESSOR} STREQUAL "aarch64")
  list(APPEND NEO_CORE_UTILITIES_LINUX
       ${CMAKE_CURRENT_SOURCE_DIR}/cpu_info.cpp
  )

  set_property(GLOBAL PROPERTY NEO_CORE_UTILITIES_LINUX ${NEO_CORE_UTILITIES_LINUX})
endif()
