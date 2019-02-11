/*
 * Copyright (C) 2018-2019 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "unit_tests/mocks/mock_aub_manager.h"

namespace OCLRT {
aub_stream::AubManager *createAubManager(uint32_t productFamily, uint32_t devicesCount, uint64_t memoryBankSize, bool localMemorySupported, const std::string &aubFileName, uint32_t streamMode) {
    return new MockAubManager(productFamily, devicesCount, memoryBankSize, localMemorySupported, aubFileName, streamMode);
}
} // namespace OCLRT
