/*
 * Copyright (C) 2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once
#include "shared/source/memory_manager/graphics_allocation.h"

#include "level_zero/core/source/device/device.h"
#include "level_zero/core/source/mutable_cmdlist/mcl_types.h"
namespace L0::MCL {
ze_result_t getBufferGpuAddress(void *buffer, L0::Device *device, NEO::GraphicsAllocation *&outGraphicsAllocation, GpuAddress &outGPUAddress);
} // namespace L0::MCL
