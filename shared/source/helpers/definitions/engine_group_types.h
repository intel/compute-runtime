/*
 * Copyright (C) 2020-2023 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once

#include <cstdint>

namespace NEO {

enum class EngineGroupType : uint32_t {
    compute = 0,
    renderCompute,
    copy,
    linkedCopy,
    cooperativeCompute,
    maxEngineGroups
};

struct EngineHelper {
    static bool isCopyOnlyEngineType(EngineGroupType type) {
        return (EngineGroupType::copy == type || EngineGroupType::linkedCopy == type);
    }
};

} // namespace NEO
