/*
 * Copyright (C) 2020 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "unit_tests/mocks/mock_platform.h"
namespace NEO {

bool initPlatform() {
    auto pPlatform = platform();
    auto executionEnvironment = pPlatform->peekExecutionEnvironment();
    size_t numRootDevices = 0u;
    getDevices(numRootDevices, *executionEnvironment);
    return pPlatform->initialize(numRootDevices, 0u);
}
} // namespace NEO
