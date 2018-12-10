/*
 * Copyright (C) 2018 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "unit_tests/mocks/mock_aub_manager.h"

namespace OCLRT {
AubManager *createAubManager(uint32_t gfxFamily, uint32_t devicesCount, size_t memoryBankSizeInGB, bool localMemorySupported, uint32_t deviceId, const std::string &aubFileName) {
    return new MockAubManager();
}
} // namespace OCLRT
