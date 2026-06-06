/*
 * Copyright (C) 2026 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once

#include <cstdint>

namespace NEO {

enum class FabricVertexType : uint16_t {
    device = 1,
    subdevice = 2,
    switchType = 3,
};

struct FabricVertexInfo {
    uint16_t fabricId;
    FabricVertexType type;
    uint8_t uuid[16];
    uint32_t pciBdf;
};

} // namespace NEO
