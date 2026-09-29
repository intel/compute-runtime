/*
 * Copyright (C) 2026 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once

#include <cstdint>

namespace NEO {
class GraphicsAllocation;
} // namespace NEO

namespace L0 {

using PatchPreambleCounter = uint64_t;
using PatchPreambleHostAddress = uint64_t *;
using PatchPreambleHostGpuAddress = uint64_t;
using PatchPreambleHostGraphicsAllocation = NEO::GraphicsAllocation *;
using PatchPreambleDeviceGpuAddress = uint64_t;
using PatchPreambleDeviceGraphicsAllocation = NEO::GraphicsAllocation *;

} // namespace L0
