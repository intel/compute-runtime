#
# Copyright (C) 2020-2021 Intel Corporation
#
# SPDX-License-Identifier: MIT
#

target_sources(${TARGET_NAME} PRIVATE
               ${CMAKE_CURRENT_SOURCE_DIR}/CMakeLists.txt
               ${CMAKE_CURRENT_SOURCE_DIR}/direct_submission_controller_mock.h
               ${CMAKE_CURRENT_SOURCE_DIR}/direct_submission_controller_tests.cpp
               ${CMAKE_CURRENT_SOURCE_DIR}/direct_submission_tests_1.cpp
               ${CMAKE_CURRENT_SOURCE_DIR}/direct_submission_tests_2.cpp
)

if(TESTS_XE_HP_CORE)
  target_sources(${TARGET_NAME} PRIVATE
                 ${CMAKE_CURRENT_SOURCE_DIR}/direct_submission_tests_xe_hp_core.cpp
  )
endif()

add_subdirectories()
