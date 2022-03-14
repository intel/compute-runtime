#
# Copyright (C) 2021-2022 Intel Corporation
#
# SPDX-License-Identifier: MIT
#

if(TESTS_PVC)
  set(NEO_CORE_TESTS_XE_HPC_CORE_PVC
      ${CMAKE_CURRENT_SOURCE_DIR}/CMakeLists.txt
      ${CMAKE_CURRENT_SOURCE_DIR}/test_encode_dispatch_kernel_pvc.cpp
      ${CMAKE_CURRENT_SOURCE_DIR}/test_preamble_pvc.cpp
      ${CMAKE_CURRENT_SOURCE_DIR}/test_sample_pvc.cpp
  )

  target_sources(${TARGET_NAME} PRIVATE ${NEO_CORE_TESTS_XE_HPC_CORE_PVC})
endif()
