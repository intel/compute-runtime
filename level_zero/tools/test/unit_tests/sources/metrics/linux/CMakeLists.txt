#
# Copyright (C) 2020-2022 Intel Corporation
#
# SPDX-License-Identifier: MIT
#

set(L0_TOOLS_METRICS_LINUX_TESTS_LINUX
    ${CMAKE_CURRENT_SOURCE_DIR}/CMakeLists.txt
    ${CMAKE_CURRENT_SOURCE_DIR}/test_metric_oa_query_pool_linux.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/test_metric_ip_sampling_linux.cpp
)

if(TESTS_PVC)
  if(NEO_ENABLE_i915_PRELIM_DETECTION)
    list(APPEND L0_TOOLS_METRICS_LINUX_TESTS_LINUX
         ${CMAKE_CURRENT_SOURCE_DIR}/test_metric_ip_sampling_linux_pvc_prelim.cpp
    )
  endif()

  if("${BRANCH_TYPE}" STREQUAL "")
    list(APPEND L0_TOOLS_METRICS_LINUX_TESTS_LINUX
         ${CMAKE_CURRENT_SOURCE_DIR}/test_metric_ip_sampling_linux_pvc_upstream.cpp
    )
  endif()
endif()

if(UNIX)
  target_sources(${TARGET_NAME} PRIVATE
                 ${L0_TOOLS_METRICS_LINUX_TESTS_LINUX}
  )
endif()
