#
# Copyright (C) 2019 Intel Corporation
#
# SPDX-License-Identifier: MIT
#

set(NEO_CORE_HELPERS
  ${CMAKE_CURRENT_SOURCE_DIR}/CMakeLists.txt
  ${CMAKE_CURRENT_SOURCE_DIR}/abort.h
  ${CMAKE_CURRENT_SOURCE_DIR}/aligned_memory.h
  ${CMAKE_CURRENT_SOURCE_DIR}/basic_math.h
  ${CMAKE_CURRENT_SOURCE_DIR}/common_types.h
  ${CMAKE_CURRENT_SOURCE_DIR}/debug_helpers.h
  ${CMAKE_CURRENT_SOURCE_DIR}/deferred_deleter_helper.h
  ${CMAKE_CURRENT_SOURCE_DIR}/extendable_enum.h
  ${CMAKE_CURRENT_SOURCE_DIR}/file_io.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/file_io.h
  ${CMAKE_CURRENT_SOURCE_DIR}/hash.h
  ${CMAKE_CURRENT_SOURCE_DIR}/hw_helper.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/hw_helper.h
  ${CMAKE_CURRENT_SOURCE_DIR}/hw_helper_base.inl
  ${CMAKE_CURRENT_SOURCE_DIR}/hw_helper_bdw_plus.inl
  ${CMAKE_CURRENT_SOURCE_DIR}/interlocked_max.h
  ${CMAKE_CURRENT_SOURCE_DIR}/kernel_helpers.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/kernel_helpers.h
  ${CMAKE_CURRENT_SOURCE_DIR}/non_copyable_or_moveable.h
  ${CMAKE_CURRENT_SOURCE_DIR}/pipeline_select_args.h
  ${CMAKE_CURRENT_SOURCE_DIR}/preamble.h
  ${CMAKE_CURRENT_SOURCE_DIR}/preamble_base.inl
  ${CMAKE_CURRENT_SOURCE_DIR}/preamble_bdw_plus.inl
  ${CMAKE_CURRENT_SOURCE_DIR}/ptr_math.h
  ${CMAKE_CURRENT_SOURCE_DIR}/register_offsets.h
  ${CMAKE_CURRENT_SOURCE_DIR}/simd_helper.h
  ${CMAKE_CURRENT_SOURCE_DIR}/stdio.h
  ${CMAKE_CURRENT_SOURCE_DIR}/string.h
  ${CMAKE_CURRENT_SOURCE_DIR}/vec.h
)

set_property(GLOBAL PROPERTY NEO_CORE_HELPERS ${NEO_CORE_HELPERS})
add_subdirectories()
