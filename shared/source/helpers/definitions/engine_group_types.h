/*
 * Copyright (C) 2020-2022 Intel Corporation
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

struct EngineHelper {
    static bool isCopyOnlyEngineType(EngineGroupType type) {
        return (EngineGroupType::Copy == type || EngineGroupType::LinkedCopy == type);
    }
};

} // namespace NEO
