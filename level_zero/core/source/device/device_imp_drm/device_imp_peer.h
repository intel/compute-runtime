/*
 * Copyright (C) 2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once
#include <level_zero/ze_api.h>

namespace L0 {
struct DeviceImp;

ze_result_t queryFabricStatsDrm(DeviceImp *pSourceDevice, DeviceImp *pPeerDevice, uint32_t &latency, uint32_t &bandwidth);

} // namespace L0
