#
# Copyright (C) 2019-2020 Intel Corporation
#
# SPDX-License-Identifier: MIT
#

set(NEO_CORE_KERNEL
  ${CMAKE_CURRENT_SOURCE_DIR}/CMakeLists.txt
  ${CMAKE_CURRENT_SOURCE_DIR}/debug_data.h
  ${CMAKE_CURRENT_SOURCE_DIR}/dispatch_kernel_encoder_interface.h
  ${CMAKE_CURRENT_SOURCE_DIR}/grf_config.h
  ${CMAKE_CURRENT_SOURCE_DIR}/kernel_arg_descriptor.h
  ${CMAKE_CURRENT_SOURCE_DIR}/kernel_arg_descriptor_extended_device_side_enqueue.h
  ${CMAKE_CURRENT_SOURCE_DIR}/kernel_arg_descriptor_extended_vme.h
  ${CMAKE_CURRENT_SOURCE_DIR}/kernel_arg_metadata.h
  ${CMAKE_CURRENT_SOURCE_DIR}/kernel_descriptor.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/kernel_descriptor.h
  ${CMAKE_CURRENT_SOURCE_DIR}/kernel_descriptor_from_patchtokens.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/kernel_descriptor_from_patchtokens.h
)

set_property(GLOBAL PROPERTY NEO_CORE_KERNEL ${NEO_CORE_KERNEL})
add_subdirectories()
