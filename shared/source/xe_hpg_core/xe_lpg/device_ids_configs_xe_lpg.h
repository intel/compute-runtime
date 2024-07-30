/*
 * Copyright (C) 2022-2024 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once

#include <vector>

//

#include "xe_lpg/device_ids_configs_xe_lpg_additional.inl"

namespace NEO {
const std::vector<unsigned short> mtlmDeviceIds{
    0x7D40,
    0x7D45,
    0x7D67,
    0x7D41};
const std::vector<unsigned short> mtlpDeviceIds{
    0x7D55,
    0x7DD5};
} // namespace NEO
