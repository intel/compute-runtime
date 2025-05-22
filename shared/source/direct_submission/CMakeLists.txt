#
# Copyright (C) 2020-2025 Intel Corporation
#
# SPDX-License-Identifier: MIT
#

set(NEO_CORE_DIRECT_SUBMISSION
    ${CMAKE_CURRENT_SOURCE_DIR}/CMakeLists.txt
    ${CMAKE_CURRENT_SOURCE_DIR}/direct_submission_prefetcher_base.inl
    ${CMAKE_CURRENT_SOURCE_DIR}/direct_submission_prefetch_mitigation_base.inl
    ${CMAKE_CURRENT_SOURCE_DIR}/direct_submission_controller.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/direct_submission_controller.h
    ${CMAKE_CURRENT_SOURCE_DIR}/direct_submission_hw.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/direct_submission_hw.h
    ${CMAKE_CURRENT_SOURCE_DIR}/direct_submission_hw.inl
    ${CMAKE_CURRENT_SOURCE_DIR}/direct_submission_tgllp_and_later.inl
    ${CMAKE_CURRENT_SOURCE_DIR}/direct_submission_properties.h
    ${CMAKE_CURRENT_SOURCE_DIR}/direct_submission_relaxed_ordering.inl
    ${CMAKE_CURRENT_SOURCE_DIR}/relaxed_ordering_helper.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/relaxed_ordering_helper.h
)

if(SUPPORT_XEHP_AND_LATER)
  list(APPEND NEO_CORE_DIRECT_SUBMISSION
       ${CMAKE_CURRENT_SOURCE_DIR}/direct_submission_xe_hp_core_and_later.inl
       ${CMAKE_CURRENT_SOURCE_DIR}/direct_submission_prefetcher_xe_hp_core_and_later.inl
       ${CMAKE_CURRENT_SOURCE_DIR}/direct_submission_prefetch_mitigation_xe_hp_core_and_later.inl
  )
endif()

if(SUPPORT_PVC_AND_LATER)
  list(APPEND NEO_CORE_DIRECT_SUBMISSION
       ${CMAKE_CURRENT_SOURCE_DIR}/direct_submission_prefetcher_pvc_and_later.inl
  )
endif()

set_property(GLOBAL PROPERTY NEO_CORE_DIRECT_SUBMISSION ${NEO_CORE_DIRECT_SUBMISSION})

add_subdirectories()
