#
# Copyright (C) 2021-2022 Intel Corporation
#
# SPDX-License-Identifier: MIT
#

target_sources(neo_shared_tests PRIVATE
               ${CMAKE_CURRENT_SOURCE_DIR}/CMakeLists.txt
               ${CMAKE_CURRENT_SOURCE_DIR}/command_container_fixture.cpp
               ${CMAKE_CURRENT_SOURCE_DIR}/command_container_fixture.h
               ${CMAKE_CURRENT_SOURCE_DIR}/direct_submission_fixture.h
               ${CMAKE_CURRENT_SOURCE_DIR}/front_window_fixture.cpp
               ${CMAKE_CURRENT_SOURCE_DIR}/front_window_fixture.h
               ${CMAKE_CURRENT_SOURCE_DIR}/implicit_scaling_fixture.cpp
               ${CMAKE_CURRENT_SOURCE_DIR}/implicit_scaling_fixture.h
               ${CMAKE_CURRENT_SOURCE_DIR}/preemption_fixture.cpp
               ${CMAKE_CURRENT_SOURCE_DIR}/preemption_fixture.h
               ${CMAKE_CURRENT_SOURCE_DIR}/product_config_fixture.h
               ${CMAKE_CURRENT_SOURCE_DIR}/templated_fixture_tests.cpp
)