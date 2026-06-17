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

bool queryFabricStatsDrm(Device &sourceDevice, Device &peerDevice, uint32_t &latency, uint32_t &bandwidth);
bool queryFabricStatsIaf(Device &sourceDevice, Device &peerDevice, uint32_t &latency, uint32_t &bandwidth);
bool queryPeerAccessDrm(Device &device, Device &peerDevice, GraphicsAllocation **probeAllocPtr, uint64_t *handle);

} // namespace NEO
