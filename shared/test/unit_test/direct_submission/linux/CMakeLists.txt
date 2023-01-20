#
# Copyright (C) 2020-2022 Intel Corporation
#
# SPDX-License-Identifier: MIT
#

set(NEO_CORE_DIRECT_SUBMISSION_LINUX_TESTS
    ${CMAKE_CURRENT_SOURCE_DIR}/CMakeLists.txt
    ${CMAKE_CURRENT_SOURCE_DIR}/drm_direct_submission_tests.cpp
)

set_property(GLOBAL PROPERTY NEO_CORE_DIRECT_SUBMISSION_LINUX_TESTS ${NEO_CORE_DIRECT_SUBMISSION_LINUX_TESTS})

if(UNIX)
  target_sources(neo_shared_tests PRIVATE ${NEO_CORE_DIRECT_SUBMISSION_LINUX_TESTS})
endif()
