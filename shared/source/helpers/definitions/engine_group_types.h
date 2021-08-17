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
    RenderCompute = 0,
    Compute,
    Copy,
    MaxEngineGroups
};

} // namespace NEO
