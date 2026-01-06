/*
 * Copyright (C) 2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once
#include "level_zero/core/source/mutable_cmdlist/mcl_types.h"
#include "level_zero/ze_api.h"

namespace NEO {
class GraphicsAllocation;
} // namespace NEO

namespace L0 {
struct Device;

namespace MCL {
ze_result_t getBufferGpuAddress(void *buffer, L0::Device *device, NEO::GraphicsAllocation *&outGraphicsAllocation, GpuAddress &outGpuAddress);
} // namespace MCL

} // namespace L0
