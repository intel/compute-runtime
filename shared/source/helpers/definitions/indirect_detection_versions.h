/*
 * Copyright (C) 2024 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once

#include <cstdint>

namespace NEO {
namespace IndirectDetectionVersions {
constexpr uint32_t disabled = std::numeric_limits<uint32_t>::max();
constexpr uint32_t requiredDetectIndirectVersionPVC = 3u;
constexpr uint32_t requiredDetectIndirectVersionPVCVectorCompiler = disabled;
constexpr uint32_t requiredDetectIndirectVersionBindless = 4u;
constexpr uint32_t requiredDetectIndirectVersionVectorCompiler = 6u;
} // namespace IndirectDetectionVersions
} // namespace NEO