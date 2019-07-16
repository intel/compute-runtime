/*
 * Copyright (C) 2017-2019 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once
#include <cstdint>

namespace NEO {

constexpr uint32_t numGpgpuEngineInstances = 3u;
constexpr uint32_t maxOsContextCount = numGpgpuEngineInstances;
constexpr uint32_t maxHandleCount = 1u;

} // namespace NEO
