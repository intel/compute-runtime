#
# Copyright (C) 2020 Intel Corporation
#
# SPDX-License-Identifier: MIT
#

set(NEO_CORE_DIRECT_SUBMISSION_DISPATCHERS
  ${CMAKE_CURRENT_SOURCE_DIR}/CMakeLists.txt
  ${CMAKE_CURRENT_SOURCE_DIR}/blitter_dispatcher.h
  ${CMAKE_CURRENT_SOURCE_DIR}/blitter_dispatcher.inl
  ${CMAKE_CURRENT_SOURCE_DIR}/dispatcher.h
  ${CMAKE_CURRENT_SOURCE_DIR}/render_dispatcher.h
  ${CMAKE_CURRENT_SOURCE_DIR}/render_dispatcher.inl
)

set_property(GLOBAL PROPERTY NEO_CORE_DIRECT_SUBMISSION_DISPATCHERS ${NEO_CORE_DIRECT_SUBMISSION_DISPATCHERS})
add_subdirectories()