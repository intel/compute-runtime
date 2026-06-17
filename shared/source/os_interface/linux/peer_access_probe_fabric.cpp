/*
 * Copyright (C) 2026 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/os_interface/linux/peer_access_probe.h"

namespace NEO {

bool queryFabricStatsDrm(Device &sourceDevice, Device &peerDevice, uint32_t &latency, uint32_t &bandwidth) {
    return queryFabricStatsIaf(sourceDevice, peerDevice, latency, bandwidth);
}

} // namespace NEO
