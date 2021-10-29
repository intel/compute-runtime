#
# Copyright (C) 2019-2021 Intel Corporation
#
# SPDX-License-Identifier: MIT
#

target_sources(${TARGET_NAME} PRIVATE
               ${CMAKE_CURRENT_SOURCE_DIR}/CMakeLists.txt
               ${CMAKE_CURRENT_SOURCE_DIR}/command_container_fixture.h
               ${CMAKE_CURRENT_SOURCE_DIR}/direct_submission_fixture.h
               ${CMAKE_CURRENT_SOURCE_DIR}/front_window_fixture.cpp
               ${CMAKE_CURRENT_SOURCE_DIR}/front_window_fixture.h
               ${CMAKE_CURRENT_SOURCE_DIR}/implicit_scaling_fixture.cpp
               ${CMAKE_CURRENT_SOURCE_DIR}/implicit_scaling_fixture.h
               ${CMAKE_CURRENT_SOURCE_DIR}/linear_stream_fixture.h
               ${CMAKE_CURRENT_SOURCE_DIR}/preemption_fixture.cpp
               ${CMAKE_CURRENT_SOURCE_DIR}/preemption_fixture.h
)

add_subdirectories()
