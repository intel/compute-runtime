/*
 * Copyright (C) 2026 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once
#include <cstdint>
#include <span>
namespace NEO {

namespace ThreadDataHash {
uint64_t computeThreadDataHash(std::span<const uint8_t> crossThreadData, std::span<const uint8_t> perThreadData);
}; // namespace ThreadDataHash

} // namespace NEO
