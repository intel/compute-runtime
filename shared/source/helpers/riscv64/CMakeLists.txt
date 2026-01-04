#
# Copyright (C) 2025 Intel Corporation
#
# SPDX-License-Identifier: MIT
#

if(${NEO_TARGET_PROCESSOR} STREQUAL "riscv64")
  list(APPEND NEO_CORE_HELPERS
       ${CMAKE_CURRENT_SOURCE_DIR}/CMakeLists.txt
       ${CMAKE_CURRENT_SOURCE_DIR}/local_id_gen.cpp
  )

  set_property(GLOBAL PROPERTY NEO_CORE_HELPERS ${NEO_CORE_HELPERS})
endif()
