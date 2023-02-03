/*
 * Copyright (C) 2020-2023 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/test/common/os_interface/windows/wddm_fixture.h"
#include "shared/test/common/test_macros/hw_test.h"

namespace NEO {
std::unique_ptr<HwDeviceIdWddm> createHwDeviceIdFromAdapterLuid(OsEnvironmentWin &osEnvironment, LUID adapterLuid, uint32_t adapterNodeOrdinal);

using WddmOsContextDeviceLuidTests = WddmFixtureLuid;
TEST_F(WddmFixtureLuid, givenValidOsContextAndLuidDataRequestThenValidDataReturned) {
    LUID adapterLuid = {0x12, 0x1234};
    wddm->hwDeviceId = NEO::createHwDeviceIdFromAdapterLuid(*osEnvironment, adapterLuid, 1u);
    std::vector<uint8_t> luidData;
    size_t arraySize = 8;
    osContext->getDeviceLuidArray(luidData, arraySize);
    uint64_t luid = 0;
    memcpy_s(&luid, sizeof(uint64_t), luidData.data(), sizeof(uint8_t) * luidData.size());
    EXPECT_NE(luid, (uint64_t)0);
}
TEST_F(WddmFixtureLuid, givenValidOsContextAndGetDeviceNodeMaskThenValidDataReturned) {
    LUID adapterLuid = {0x12, 0x1234};
    wddm->hwDeviceId = NEO::createHwDeviceIdFromAdapterLuid(*osEnvironment, adapterLuid, 2u);
    uint32_t adapterDeviceMask = osContext->getDeviceNodeMask();
    EXPECT_EQ(adapterDeviceMask, (uint32_t)4);
}
} // namespace NEO