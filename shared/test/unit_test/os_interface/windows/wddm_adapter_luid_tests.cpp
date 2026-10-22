/*
 * Copyright (C) 2020-2026 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/test/common/helpers/debug_manager_state_restore.h"
#include "shared/test/common/helpers/variable_backup.h"
#include "shared/test/common/os_interface/windows/wddm_fixture.h"
#include "shared/test/common/test_macros/test.h"

#include "gtest/gtest.h"

#include <memory>
#include <vector>

namespace NEO {
std::unique_ptr<HwDeviceIdWddm> createHwDeviceIdFromAdapterLuid(OsEnvironmentWin &osEnvironment, LUID adapterLuid, uint32_t adapterNodeOrdinal);

namespace SysCalls {
extern const wchar_t *currentLibraryPath;
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

    VariableBackup<const wchar_t *> driverStorePathBackup{&queryRegistryDriverStorePath, mockAdapterDriverStorePath.data()};
    VariableBackup<const wchar_t *> currentLibraryPathBackup{&SysCalls::currentLibraryPath, mockLibraryFromOtherDriverStore.data()};

    LUID adapterLuid = {0x1, 0x1};
    wddm->hwDeviceId = NEO::createHwDeviceIdFromAdapterLuid(*osEnvironment, adapterLuid, 2u);
    EXPECT_EQ(nullptr, wddm->hwDeviceId);

    debugManager.flags.DoNotValidateDriverPath.set(1);

    wddm->hwDeviceId = NEO::createHwDeviceIdFromAdapterLuid(*osEnvironment, adapterLuid, 2u);
    EXPECT_NE(nullptr, wddm->hwDeviceId);
}

TEST_F(WddmFixtureLuid, givenOverriddenGdiPathWhenCreatingHwDeviceThenDriverStorePathIsNotValidated) {
    DebugManagerStateRestore debugSettingsRestore;

    VariableBackup<const wchar_t *> driverStorePathBackup{&queryRegistryDriverStorePath, mockAdapterDriverStorePath.data()};
    VariableBackup<const wchar_t *> currentLibraryPathBackup{&SysCalls::currentLibraryPath, mockLibraryFromOtherDriverStore.data()};

    LUID adapterLuid = {0x1, 0x1};
    wddm->hwDeviceId = NEO::createHwDeviceIdFromAdapterLuid(*osEnvironment, adapterLuid, 2u);
    EXPECT_EQ(nullptr, wddm->hwDeviceId);

    debugManager.flags.OverrideGdiPath.set("customGdi.dll");

    wddm->hwDeviceId = NEO::createHwDeviceIdFromAdapterLuid(*osEnvironment, adapterLuid, 2u);
    EXPECT_NE(nullptr, wddm->hwDeviceId);
}

TEST_F(WddmFixtureLuid, givenAdapterFromSameDriverStoreWhenCreatingHwDeviceThenHwDeviceIsCreatedWithoutQueryingPrivateDriverData) {
    VariableBackup<const wchar_t *> driverStorePathBackup{&queryRegistryDriverStorePath, mockAdapterDriverStorePath.data()};
    VariableBackup<const wchar_t *> currentLibraryPathBackup{&SysCalls::currentLibraryPath, mockLibraryFromAdapterDriverStore.data()};
    VariableBackup<uint32_t> privateQueryCountBackup{&queryAdapterInfoUmDriverPrivateCallCount, 0u};

    LUID adapterLuid = {0x1, 0x1};
    auto hwDeviceId = NEO::createHwDeviceIdFromAdapterLuid(*osEnvironment, adapterLuid, 2u);
    EXPECT_NE(nullptr, hwDeviceId);
    EXPECT_EQ(0u, queryAdapterInfoUmDriverPrivateCallCount);
}

TEST_F(WddmFixtureLuid, givenAdapterFromOtherDriverStoreWhenCreatingHwDeviceThenHwDeviceIsNotCreatedAndPrivateDriverDataIsNotQueried) {
    VariableBackup<const wchar_t *> driverStorePathBackup{&queryRegistryDriverStorePath, mockAdapterDriverStorePath.data()};
    VariableBackup<const wchar_t *> currentLibraryPathBackup{&SysCalls::currentLibraryPath, mockLibraryFromOtherDriverStore.data()};
    VariableBackup<uint32_t> privateQueryCountBackup{&queryAdapterInfoUmDriverPrivateCallCount, 0u};

    LUID adapterLuid = {0x1, 0x1};
    auto hwDeviceId = NEO::createHwDeviceIdFromAdapterLuid(*osEnvironment, adapterLuid, 2u);
    EXPECT_EQ(nullptr, hwDeviceId);
    EXPECT_EQ(0u, queryAdapterInfoUmDriverPrivateCallCount);
}

TEST_F(WddmFixtureLuid, givenDriverStorePathQueryFailsWhenCreatingHwDeviceThenHwDeviceIsNotCreated) {
    VariableBackup<const wchar_t *> driverStorePathBackup{&queryRegistryDriverStorePath, nullptr};
    VariableBackup<const wchar_t *> currentLibraryPathBackup{&SysCalls::currentLibraryPath, mockLibraryFromAdapterDriverStore.data()};

    LUID adapterLuid = {0x1, 0x1};
    auto hwDeviceId = NEO::createHwDeviceIdFromAdapterLuid(*osEnvironment, adapterLuid, 2u);
    EXPECT_EQ(nullptr, hwDeviceId);
}

} // namespace NEO
