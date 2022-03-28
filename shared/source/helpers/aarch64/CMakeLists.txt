#
# Copyright (C) 2019-2022 Intel Corporation
#
# SPDX-License-Identifier: MIT
#

if(${NEO_TARGET_PROCESSOR} STREQUAL "aarch64")
  list(APPEND NEO_CORE_HELPERS
       ${CMAKE_CURRENT_SOURCE_DIR}/CMakeLists.txt
       ${CMAKE_CURRENT_SOURCE_DIR}/local_id_gen.cpp
  )

  if(COMPILER_SUPPORTS_NEON)
    list(APPEND NEO_CORE_HELPERS
         ${CMAKE_CURRENT_SOURCE_DIR}/local_id_gen_neon.cpp
         ${CMAKE_CURRENT_SOURCE_DIR}/uint16_neon.h
    )
  endif()

  set_property(GLOBAL PROPERTY NEO_CORE_HELPERS ${NEO_CORE_HELPERS})
endif()
