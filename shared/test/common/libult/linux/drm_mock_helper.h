/*
 * Copyright (C) 2022 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once

#include "shared/source/helpers/basic_math.h"

#include "gtest/gtest.h"

namespace DrmMockHelper {

inline uint32_t getTileFromEngineOrMemoryInstance(uint16_t instanceValue) {
    uint8_t tileMask = (instanceValue >> 8);
    return Math::log2(static_cast<uint64_t>(tileMask));
}

inline uint16_t getEngineOrMemoryInstanceValue(uint16_t tile, uint16_t id) {
    EXPECT_TRUE(id < 256);
    uint16_t tileMask = ((1 << tile) << 8);
    return (id | tileMask);
}

inline uint16_t getIdFromEngineOrMemoryInstance(uint16_t instanceValue) {
    return (instanceValue & 0b11111111);
}

} // namespace DrmMockHelper
