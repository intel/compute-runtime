#
# Copyright (C) 2024 Intel Corporation
#
# SPDX-License-Identifier: MIT
#

target_sources(${L0_STATIC_LIB_NAME}
               PRIVATE
               ${CMAKE_CURRENT_SOURCE_DIR}/CMakeLists.txt
               ${CMAKE_CURRENT_SOURCE_DIR}/ze_intel_gpu.h
               ${CMAKE_CURRENT_SOURCE_DIR}/zet_intel_gpu_debug.h
               ${CMAKE_CURRENT_SOURCE_DIR}/zet_intel_gpu_metric_export.h
)

add_subdirectories()

