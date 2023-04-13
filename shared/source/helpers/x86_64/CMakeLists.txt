#
# Copyright (C) 2019-2023 Intel Corporation
#
# SPDX-License-Identifier: MIT
#

if(${NEO_TARGET_PROCESSOR} STREQUAL "x86_64")
  set(NEO_CORE_HELPERS
      ${CMAKE_CURRENT_SOURCE_DIR}/CMakeLists.txt
      ${CMAKE_CURRENT_SOURCE_DIR}/local_id_gen.cpp
      ${CMAKE_CURRENT_SOURCE_DIR}/local_id_gen_avx2.cpp
  )

  set_property(GLOBAL APPEND PROPERTY NEO_CORE_HELPERS ${NEO_CORE_HELPERS})
endif()
