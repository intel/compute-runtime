/*
 * Copyright (C) 2019-2020 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once

#include "engine_node.h"

namespace NEO {
struct HardwareInfo;

namespace EngineHelpers {
bool isCcs(aub_stream::EngineType engineType);
bool isBcs(aub_stream::EngineType engineType);
aub_stream::EngineType getBcsEngineType(const HardwareInfo &hwInfo);
}; // namespace EngineHelpers
} // namespace NEO
