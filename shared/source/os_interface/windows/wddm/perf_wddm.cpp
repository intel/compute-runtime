/*
 * Copyright (C) 2024 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/os_interface/windows/wddm/wddm.h"

namespace NEO {

bool Wddm::perfOpenEuStallStream(uint32_t sampleRate, uint32_t minBufferSize) {
    return false;
}

bool Wddm::perfReadEuStallStream(uint8_t *pRawData, size_t *pRawDataSize) {
    return false;
}

bool Wddm::perfDisableEuStallStream() {
    return false;
}
} // namespace NEO