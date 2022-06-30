/*
 * Copyright (C) 2021-2022 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/test/unit_test/os_interface/windows/adapter_info_tests.h"

#include "shared/source/os_interface/windows/wddm/adapter_factory.h"
#include "shared/source/os_interface/windows/wddm/adapter_factory_dxcore.h"
#include "shared/source/os_interface/windows/wddm/adapter_factory_dxgi.h"
#include "shared/source/os_interface/windows/wddm/adapter_info.h"
#include "shared/test/common/helpers/debug_manager_state_restore.h"
#include "shared/test/common/os_interface/windows/ult_dxcore_factory.h"
#include "shared/test/common/os_interface/windows/ult_dxgi_factory.h"
#include "shared/test/common/test_macros/test.h"

#include <memory>

HRESULT(WINAPI failCreateTopLevelInterface)
(REFIID riid, void **ppFactory) {
    return -1;
}

template <typename T>
HRESULT(WINAPI createTopLevelInterface)
(REFIID riid, void **ppFactory) {
    auto obj = std::make_unique<T>();
    *reinterpret_cast<T **>(ppFactory) = obj.get();
    obj.release();
    return S_OK;
}

TEST(AdapterInfoTest, whenQueryingForDriverStorePathThenProvidesEnoughMemoryForReturnString) {
    NEO::Gdi gdi;
    auto handle = validHandle;
    gdi.queryAdapterInfo.mFunc = QueryAdapterInfoMock::queryadapterinfo;

    auto ret = NEO::queryAdapterDriverStorePath(gdi, handle);

    EXPECT_EQ(std::wstring(driverStorePathStr), ret) << "Expected " << driverStorePathStr << ", got : " << ret;
}

TEST(DxCoreAdapterFactory, whenCouldNotCreateAdapterFactoryThenReturnsUnsupported) {
    {
        NEO::DxCoreAdapterFactory adapterFactor{nullptr};
        EXPECT_FALSE(adapterFactor.isSupported());
    }

    {
        NEO::DxCoreAdapterFactory adapterFactory{failCreateTopLevelInterface};
        EXPECT_FALSE(adapterFactory.isSupported());
    }
}

TEST(DxCoreAdapterFactory, whenCreatedAdapterFactoryThenReturnsSupported) {
    NEO::DxCoreAdapterFactory adapterFactory{&createTopLevelInterface<NEO::UltDXCoreAdapterFactory>};
    EXPECT_TRUE(adapterFactory.isSupported());
}

TEST(DxCoreAdapterFactory, whenSupportedButSnapshotOfAdaptersNotCreatedThenReturnsEmptyListOfAdapters) {
    NEO::DxCoreAdapterFactory adapterFactory{&createTopLevelInterface<NEO::UltDXCoreAdapterFactory>};
    EXPECT_EQ(0U, adapterFactory.getNumAdaptersInSnapshot());

    NEO::AdapterFactory::AdapterDesc adapterDesc;
    EXPECT_FALSE(adapterFactory.getAdapterDesc(0, adapterDesc));
}

TEST(DxCoreAdapterFactory, whenSupportedThenGiveAccessToUnderlyingAdapterDesc) {
    NEO::DxCoreAdapterFactory adapterFactory{&createTopLevelInterface<NEO::UltDXCoreAdapterFactory>};

    bool createdSnapshot = adapterFactory.createSnapshotOfAvailableAdapters();
    EXPECT_TRUE(createdSnapshot);
    EXPECT_EQ(1U, adapterFactory.getNumAdaptersInSnapshot());

    NEO::AdapterFactory::AdapterDesc adapterDesc;
    bool retreivedAdapterDesc = adapterFactory.getAdapterDesc(0, adapterDesc);
    EXPECT_TRUE(retreivedAdapterDesc);
    EXPECT_EQ(NEO::AdapterFactory::AdapterDesc::Type::Hardware, adapterDesc.type);
    EXPECT_EQ(0x1234U, adapterDesc.deviceId);
    EXPECT_EQ(0x1234U, adapterDesc.luid.HighPart);
    EXPECT_EQ(0U, adapterDesc.luid.LowPart);
    EXPECT_STREQ("Intel", adapterDesc.driverDescription.c_str());
}

TEST(DxgiAdapterFactory, whenCouldNotCreateAdapterFactoryThenReturnsUnsupported) {
    {
        NEO::DxgiAdapterFactory adapterFactory{nullptr};
        EXPECT_FALSE(adapterFactory.isSupported());
    }

    {
        NEO::DxgiAdapterFactory adapterFactory{failCreateTopLevelInterface};
        EXPECT_FALSE(adapterFactory.isSupported());
    }
}

TEST(DxgiAdapterFactory, whenCreatedAdapterFactoryThenReturnsSupported) {
    NEO::DxgiAdapterFactory adapterFactory{&createTopLevelInterface<NEO::UltIDXGIFactory1>};
    EXPECT_TRUE(adapterFactory.isSupported());
}

TEST(DxgiAdapterFactory, whenSupportedButSnapshotOfAdaptersNotCreatedThenReturnsEmptyListOfAdapters) {
    NEO::DxgiAdapterFactory adapterFactory{&createTopLevelInterface<NEO::UltIDXGIFactory1>};
    EXPECT_EQ(0U, adapterFactory.getNumAdaptersInSnapshot());

    NEO::AdapterFactory::AdapterDesc adapterDesc;
    EXPECT_FALSE(adapterFactory.getAdapterDesc(0, adapterDesc));
}

TEST(DxgiAdapterFactory, whenSupportedThenGiveAccessToUnderlyingAdapterDesc) {
    NEO::DxgiAdapterFactory adapterFactory{&createTopLevelInterface<NEO::UltIDXGIFactory1>};

    bool createdSnapshot = adapterFactory.createSnapshotOfAvailableAdapters();
    EXPECT_TRUE(createdSnapshot);
    EXPECT_EQ(1U, adapterFactory.getNumAdaptersInSnapshot());

    NEO::AdapterFactory::AdapterDesc adapterDesc;
    bool retreivedAdapterDesc = adapterFactory.getAdapterDesc(0, adapterDesc);
    EXPECT_TRUE(retreivedAdapterDesc);
    EXPECT_EQ(NEO::AdapterFactory::AdapterDesc::Type::Unknown, adapterDesc.type);
    EXPECT_EQ(0x1234U, adapterDesc.deviceId);
    EXPECT_EQ(0x1234U, adapterDesc.luid.HighPart);
    EXPECT_EQ(0U, adapterDesc.luid.LowPart);
    EXPECT_STREQ("Intel", adapterDesc.driverDescription.c_str());
}

TEST(IsAllowedDeviceId, whenDebugKeyNotSetThenReturnTrue) {
    EXPECT_TRUE(NEO::isAllowedDeviceId(0xdeadbeef));
}

TEST(IsAllowedDeviceId, whenForceDeviceIdDebugKeySetThenExpectSpecificValue) {
    DebugManagerStateRestore rest;
    DebugManager.flags.ForceDeviceId.set("167");
    EXPECT_FALSE(NEO::isAllowedDeviceId(0xdeadbeef));

    DebugManager.flags.ForceDeviceId.set("1678");
    EXPECT_FALSE(NEO::isAllowedDeviceId(0x167));

    DebugManager.flags.ForceDeviceId.set("167");
    EXPECT_TRUE(NEO::isAllowedDeviceId(0x167));
}

TEST(IsAllowedDeviceId, whenForceDeviceIdDebugKeySetThenTreatAsHex) {
    DebugManagerStateRestore rest;

    DebugManager.flags.ForceDeviceId.set("167");
    EXPECT_FALSE(NEO::isAllowedDeviceId(167));

    DebugManager.flags.ForceDeviceId.set("167");
    EXPECT_TRUE(NEO::isAllowedDeviceId(0x167));
}

TEST(IsAllowedDeviceId, whenFilterDeviceIdDebugKeySetThenExpectSpecificValue) {
    DebugManagerStateRestore rest;
    DebugManager.flags.FilterDeviceId.set("167");
    EXPECT_FALSE(NEO::isAllowedDeviceId(0xdeadbeef));

    DebugManager.flags.FilterDeviceId.set("1678");
    EXPECT_FALSE(NEO::isAllowedDeviceId(0x167));

    DebugManager.flags.FilterDeviceId.set("167");
    EXPECT_TRUE(NEO::isAllowedDeviceId(0x167));
}

TEST(IsAllowedDeviceId, whenFilterDeviceIdDebugKeySetThenTreatAsHex) {
    DebugManagerStateRestore rest;

    DebugManager.flags.FilterDeviceId.set("167");
    EXPECT_FALSE(NEO::isAllowedDeviceId(167));

    DebugManager.flags.FilterDeviceId.set("167");
    EXPECT_TRUE(NEO::isAllowedDeviceId(0x167));
}
