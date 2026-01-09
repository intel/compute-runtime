/*
 * Copyright (C) 2025-2026 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/test/common/test_macros/test.h"

#include "level_zero/core/test/unit_tests/fixtures/device_fixture.h"
#include "level_zero/core/test/unit_tests/mocks/mock_device.h"

namespace L0 {
namespace ult {

using MultipleDeviceQueryPeerAccessWddmTests = Test<MultiDeviceFixture>;

TEST_F(MultipleDeviceQueryPeerAccessWddmTests, givenDeviceQueryPeerAccessThenReturnFalse) {
    L0::Device *device0 = driverHandle->devices[0];
    L0::Device *device1 = driverHandle->devices[1];

    void *handlePtr = nullptr;
    uint64_t handle = std::numeric_limits<uint64_t>::max();

    bool canAccess = MockDevice::queryPeerAccess(*device0->getNEODevice(), *device1->getNEODevice(), &handlePtr, &handle);
    EXPECT_FALSE(canAccess);
}

} // namespace ult
} // namespace L0