/*
 * Copyright (C) 2020-2021 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once

#include <cstdint>

namespace NEO {

enum class EngineGroupType : uint32_t {
    Compute = 0,
    RenderCompute,
    Copy,
    LinkedCopy,
    CooperativeCompute,
    MaxEngineGroups
};

} // namespace NEO
