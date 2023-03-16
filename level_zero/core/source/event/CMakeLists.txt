#
# Copyright (C) 2023 Intel Corporation
#
# SPDX-License-Identifier: MIT
#

target_sources(${L0_STATIC_LIB_NAME}
               PRIVATE
               ${CMAKE_CURRENT_SOURCE_DIR}/CMakeLists.txt
               ${CMAKE_CURRENT_SOURCE_DIR}/event.cpp
               ${CMAKE_CURRENT_SOURCE_DIR}/event.h
               ${CMAKE_CURRENT_SOURCE_DIR}/event_imp.h
               ${CMAKE_CURRENT_SOURCE_DIR}/event_impl.inl
)
