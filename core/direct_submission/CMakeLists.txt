#
# Copyright (C) 2020 Intel Corporation
#
# SPDX-License-Identifier: MIT
#

set(NEO_CORE_DIRECT_SUBMISSION
  ${CMAKE_CURRENT_SOURCE_DIR}/CMakeLists.txt
  ${CMAKE_CURRENT_SOURCE_DIR}/direct_submission_hw.h
  ${CMAKE_CURRENT_SOURCE_DIR}/direct_submission_hw.inl
  ${CMAKE_CURRENT_SOURCE_DIR}/direct_submission_properties.h
)

set_property(GLOBAL PROPERTY NEO_CORE_DIRECT_SUBMISSION ${NEO_CORE_DIRECT_SUBMISSION})

add_subdirectories()