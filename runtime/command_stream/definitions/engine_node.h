/*
 * Copyright (C) 2017-2019 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once
#include <cstdint>

namespace OCLRT {

enum EngineType : uint32_t {
    ENGINE_RCS = 0,
    ENGINE_BCS,
    ENGINE_VCS,
    ENGINE_VECS,
    NUM_ENGINES
};

namespace EngineInstanceConstants {
constexpr uint32_t lowPriorityGpgpuEngineIndex = 1;
constexpr uint32_t numGpgpuEngineInstances = 2;
} // namespace EngineInstanceConstants

constexpr uint32_t maxOsContextCount = EngineInstanceConstants::numGpgpuEngineInstances;
constexpr uint32_t maxHandleCount = 1u;
} // namespace OCLRT
