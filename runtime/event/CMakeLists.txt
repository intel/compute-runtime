#
# Copyright (C) 2018 Intel Corporation
#
# SPDX-License-Identifier: MIT
#

set(RUNTIME_SRCS_EVENT
  ${CMAKE_CURRENT_SOURCE_DIR}/CMakeLists.txt
  ${CMAKE_CURRENT_SOURCE_DIR}/async_events_handler.h
  ${CMAKE_CURRENT_SOURCE_DIR}/async_events_handler.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/event.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/event.h
  ${CMAKE_CURRENT_SOURCE_DIR}/event_builder.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/event_builder.h
  ${CMAKE_CURRENT_SOURCE_DIR}/event_tracker.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/event_tracker.h
  ${CMAKE_CURRENT_SOURCE_DIR}/user_event.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/user_event.h
  ${CMAKE_CURRENT_SOURCE_DIR}/hw_timestamps.h
  ${CMAKE_CURRENT_SOURCE_DIR}/perf_counter.h
)
target_sources(${NEO_STATIC_LIB_NAME} PRIVATE ${RUNTIME_SRCS_EVENT})
set_property(GLOBAL PROPERTY RUNTIME_SRCS_EVENT ${RUNTIME_SRCS_EVENT})
