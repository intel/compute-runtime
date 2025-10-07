/*
 * Copyright (C) 2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once

#include <level_zero/ze_api.h>

namespace NEO {
class Device;
}

namespace L0 {
struct DeviceImp;

ze_result_t queryFabricStatsDrm(DeviceImp *pSourceDevice, DeviceImp *pPeerDevice, uint32_t &latency, uint32_t &bandwidth);
bool queryPeerAccessDrm(NEO::Device &device, NEO::Device &peerDevice, void **handlePtr, uint64_t *handle);

} // namespace L0
