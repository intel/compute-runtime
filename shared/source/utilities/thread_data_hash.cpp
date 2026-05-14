/*
 * Copyright (C) 2026 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/utilities/thread_data_hash.h"

#include <cstring>
#include <string_view>
namespace NEO {

namespace ThreadDataHash {

uint64_t computeThreadDataHash(std::span<const uint8_t> crossThreadData, std::span<const uint8_t> perThreadData) {
    std::hash<std::string_view> hasher;
    uint64_t hash = hasher(std::string_view(reinterpret_cast<const char *>(crossThreadData.data()), crossThreadData.size()));
    if (perThreadData.size() == 0) {
        return hash;
    }
    const uint64_t perThreadHash = hasher(std::string_view(reinterpret_cast<const char *>(perThreadData.data()), perThreadData.size()));
    hash ^= perThreadHash + 0x9e3779b9u + (hash << 6) + (hash >> 2);
    return hash;
}

} // namespace ThreadDataHash

} // namespace NEO
