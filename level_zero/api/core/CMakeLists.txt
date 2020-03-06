#
# Copyright (C) 2020 Intel Corporation
#
# SPDX-License-Identifier: MIT
#

set(L0_SRCS_API
  ${CMAKE_CURRENT_SOURCE_DIR}/CMakeLists.txt
  ${CMAKE_CURRENT_SOURCE_DIR}/ze_barrier.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/ze_cl_interop.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/ze_cmdlist.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/ze_cmdqueue.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/ze_copy.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/ze_core_loader.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/ze_device.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/ze_driver.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/ze_event.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/ze_fence.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/ze_image.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/ze_memory.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/ze_module.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/ze_residency.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/ze_sampler.cpp
)
set_property(GLOBAL PROPERTY L0_SRCS_API ${L0_SRCS_API})
