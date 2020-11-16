/*
 * Copyright (C) 2020 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once

#include <stdint.h>

namespace NEO {

enum class EngineGroupType : uint32_t {
    RenderCompute = 0,
    Compute,
    Copy,
    MaxEngineGroups
};
}
