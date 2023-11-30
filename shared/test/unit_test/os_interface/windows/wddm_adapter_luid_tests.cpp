/*
 * Copyright (C) 2020-2023 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/os_interface/windows/sys_calls.h"
#include "shared/test/common/helpers/debug_manager_state_restore.h"
#include "shared/test/common/os_interface/windows/wddm_fixture.h"
#include "shared/test/common/test_macros/hw_test.h"

namespace NEO {
std::unique_ptr<HwDeviceIdWddm> createHwDeviceIdFromAdapterLuid(OsEnvironmentWin &osEnvironment, LUID adapterLuid, uint32_t adapterNodeOrdinal);

namespace SysCalls {
extern uint32_t regOpenKeySuccessCount;
extern uint32_t regQueryValueSuccessCount;
extern const wchar_t *currentLibraryPath;
extern const char *driverStorePath;
} // namespace SysCalls

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

TEST_F(WddmFixtureLuid, givenDoNotValidateDriverPathWhenCreatingHwDeviceThenDriverStorePathIsNotValidated) {
    DebugManagerStateRestore debugSettingsRestore;

    VariableBackup<uint32_t> openRegCountBackup{&SysCalls::regOpenKeySuccessCount};
    VariableBackup<uint32_t> queryRegCountBackup{&SysCalls::regQueryValueSuccessCount};
    VariableBackup<const char *> driverStorePathBackup{&SysCalls::driverStorePath};
    SysCalls::regOpenKeySuccessCount = 10;
    SysCalls::regQueryValueSuccessCount = 10;
    SysCalls::driverStorePath = "invalidPath";
    VariableBackup<const wchar_t *> currentLibraryPathBackup(&SysCalls::currentLibraryPath);
    SysCalls::currentLibraryPath = L"driverStore\\0x8086\\myLib.dll";

    LUID adapterLuid = {0x1, 0x1};
    wddm->hwDeviceId = NEO::createHwDeviceIdFromAdapterLuid(*osEnvironment, adapterLuid, 2u);
    EXPECT_EQ(nullptr, wddm->hwDeviceId);

    debugManager.flags.DoNotValidateDriverPath.set(1);

    wddm->hwDeviceId = NEO::createHwDeviceIdFromAdapterLuid(*osEnvironment, adapterLuid, 2u);
    EXPECT_NE(nullptr, wddm->hwDeviceId);
}

} // namespace NEO