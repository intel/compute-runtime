#
# Copyright (C) 2019-2022 Intel Corporation
#
# SPDX-License-Identifier: MIT
#

target_sources(neo_shared_tests PRIVATE
               ${CMAKE_CURRENT_SOURCE_DIR}/CMakeLists.txt
               ${CMAKE_CURRENT_SOURCE_DIR}/command_container_tests.cpp
               ${CMAKE_CURRENT_SOURCE_DIR}/command_encoder_tests.cpp
)

if(TESTS_DG2_AND_LATER)
  target_sources(neo_shared_tests PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/command_encoder_tests_xe_hpg_core_and_later.cpp)
endif()