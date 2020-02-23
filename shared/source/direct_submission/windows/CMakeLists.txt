#
# Copyright (C) 2020 Intel Corporation
#
# SPDX-License-Identifier: MIT
#

set(NEO_CORE_DIRECT_SUBMISSION_WINDOWS
  ${CMAKE_CURRENT_SOURCE_DIR}/CMakeLists.txt
  ${CMAKE_CURRENT_SOURCE_DIR}/wddm_direct_submission.h
  ${CMAKE_CURRENT_SOURCE_DIR}/wddm_direct_submission.inl
)

set_property(GLOBAL PROPERTY NEO_CORE_DIRECT_SUBMISSION_WINDOWS ${NEO_CORE_DIRECT_SUBMISSION_WINDOWS})

add_subdirectories()