#
# Copyright (C) 2020-2022 Intel Corporation
#
# SPDX-License-Identifier: MIT
#

target_sources(neo_shared_tests PRIVATE
               ${CMAKE_CURRENT_SOURCE_DIR}/CMakeLists.txt
               ${CMAKE_CURRENT_SOURCE_DIR}/blitter_dispatcher_tests.cpp
               ${CMAKE_CURRENT_SOURCE_DIR}/dispatcher_fixture.cpp
               ${CMAKE_CURRENT_SOURCE_DIR}/dispatcher_fixture.h
               ${CMAKE_CURRENT_SOURCE_DIR}/dispatcher_tests.cpp
               ${CMAKE_CURRENT_SOURCE_DIR}/render_dispatcher_tests.cpp
)
