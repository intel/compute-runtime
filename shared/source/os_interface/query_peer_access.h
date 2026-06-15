/*
 * Copyright (C) 2026 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once

#include <cstdint>

namespace NEO {
class Device;
class GraphicsAllocation;

bool queryPeerAccess(Device &device, Device &peerDevice, GraphicsAllocation **probeAllocPtr, uint64_t *handle);

} // namespace NEO
