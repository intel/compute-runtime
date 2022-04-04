#
# Copyright (C) 2022 Intel Corporation
#
# SPDX-License-Identifier: MIT
#

if(TESTS_PVC)
  set(NEO_TOOLS_TESTS_XE_HPC_CORE_PVC
      ${CMAKE_CURRENT_SOURCE_DIR}/CMakeLists.txt
      ${CMAKE_CURRENT_SOURCE_DIR}/test_debug_session_pvc.cpp
  )

  target_sources(${TARGET_NAME} PRIVATE ${NEO_TOOLS_TESTS_XE_HPC_CORE_PVC})
endif()
