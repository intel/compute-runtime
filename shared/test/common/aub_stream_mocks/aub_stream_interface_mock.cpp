/*
 * Copyright (C) 2018-2021 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/test/common/mocks/mock_aub_manager.h"

namespace NEO {
aub_stream::AubManager *createAubManager(uint32_t productFamily, uint32_t devicesCount, uint64_t memoryBankSize, uint32_t stepping, bool localMemorySupported, uint32_t streamMode, uint64_t gpuAddressSpace) {
    return new MockAubManager(productFamily, devicesCount, memoryBankSize, stepping, localMemorySupported, streamMode, gpuAddressSpace);
}
} // namespace NEO
