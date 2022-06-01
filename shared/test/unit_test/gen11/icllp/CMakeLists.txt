#
# Copyright (C) 2022 Intel Corporation
#
# SPDX-License-Identifier: MIT
#

if(TESTS_ICLLP)
  set(NEO_SHARED_tests_gen11_icllp
      ${CMAKE_CURRENT_SOURCE_DIR}/excludes_gen11_icllp.cpp
  )

  target_sources(${TARGET_NAME} PRIVATE ${NEO_SHARED_tests_gen11_icllp})

  add_subdirectories()
endif()
