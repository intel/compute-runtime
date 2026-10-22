/*
 * Copyright (C) 2026 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/os_interface/windows/gdi_interface.h"
#include "shared/source/os_interface/windows/wddm/adapter_factory_kmt.h"
#include "shared/test/common/helpers/variable_backup.h"
#include "shared/test/common/mock_gdi/mock_gdi.h"
#include "shared/test/common/os_interface/windows/ult_adapter_luid.h"
#include "shared/test/common/os_interface/windows/ult_dxcore_factory.h"
#include "shared/test/common/test_macros/test.h"

#include <memory>
#include <optional>
#include <string>

using namespace NEO;

namespace {

class KmtAdapterFactoryTest : public ::testing::Test {
  public:
    static constexpr uint32_t numAdapters = 2u;

    VariableBackup<uint32_t> enumAdapters3CountBackup{&mockEnumAdapters3Count, uint32_t{numAdapters}};
    VariableBackup<uint32_t> enumAdapters3CallCountBackup{&enumAdapters3CallCount, 0u};
    VariableBackup<uint32_t> failEnumAdapters3OnCallBackup{&failEnumAdapters3OnCall, 0u};
    VariableBackup<uint32_t> closeAdapterCallCountBackup{&closeAdapterCallCount, 0u};
    VariableBackup<std::optional<uint32_t>> softwareAdapterIndexBackup{&mockSoftwareEnumeratedAdapterIndex, std::nullopt};
    VariableBackup<std::optional<KMTQUERYADAPTERINFOTYPE>> failingQueryTypeBackup{&failingEnumeratedAdapterQueryType, std::nullopt};
    Gdi gdi;
    std::unique_ptr<KmtAdapterFactory> adapterFactory = std::make_unique<KmtAdapterFactory>(gdi);
};

TEST_F(KmtAdapterFactoryTest, givenComputeOnlyAdaptersWhenCreatingSnapshotThenAllAdaptersEnumeratedWithComputeOnlyFilterAreReported) {
    EXPECT_TRUE(adapterFactory->createSnapshotOfAvailableAdapters());

    EXPECT_EQ(2u, enumAdapters3CallCount);
    ASSERT_EQ(numAdapters, adapterFactory->getNumAdaptersInSnapshot());
    for (uint32_t i = 0; i < numAdapters; i++) {
        AdapterFactory::AdapterDesc adapterDesc;
        EXPECT_TRUE(adapterFactory->getAdapterDesc(i, adapterDesc));
        EXPECT_EQ(AdapterFactory::AdapterDesc::Type::hardware, adapterDesc.type);
        EXPECT_EQ(std::string(mockEnumeratedAdapterDescription), adapterDesc.driverDescription);
        EXPECT_EQ(mockEnumeratedAdapterDeviceIdBase + i, adapterDesc.deviceId);
        EXPECT_EQ(UltAdapterLuid::kmtFactoryId, adapterDesc.luid.HighPart);
        EXPECT_EQ(UltAdapterLuid::adapterIndexBase + i, adapterDesc.luid.LowPart);
    }
    EXPECT_EQ(numAdapters, closeAdapterCallCount);
}

TEST_F(KmtAdapterFactoryTest, givenSoftwareAdapterWhenCreatingSnapshotThenAdapterIsReportedAsNotHardware) {
    mockSoftwareEnumeratedAdapterIndex = 1u;

    EXPECT_TRUE(adapterFactory->createSnapshotOfAvailableAdapters());

    AdapterFactory::AdapterDesc adapterDesc;
    EXPECT_TRUE(adapterFactory->getAdapterDesc(0u, adapterDesc));
    EXPECT_EQ(AdapterFactory::AdapterDesc::Type::hardware, adapterDesc.type);
    EXPECT_TRUE(adapterFactory->getAdapterDesc(1u, adapterDesc));
    EXPECT_EQ(AdapterFactory::AdapterDesc::Type::notHardware, adapterDesc.type);
}

TEST_F(KmtAdapterFactoryTest, givenFailingAdapterQueryWhenCreatingSnapshotThenAdapterIsSkippedAndClosed) {
    for (auto queryType : {KMTQAITYPE_ADAPTERTYPE, KMTQAITYPE_PHYSICALADAPTERDEVICEIDS, KMTQAITYPE_DRIVER_DESCRIPTION}) {
        closeAdapterCallCount = 0u;
        failingEnumeratedAdapterQueryType = queryType;

        EXPECT_TRUE(adapterFactory->createSnapshotOfAvailableAdapters());

        EXPECT_EQ(0u, adapterFactory->getNumAdaptersInSnapshot());
        EXPECT_EQ(numAdapters, closeAdapterCallCount);
    }
}

TEST_F(KmtAdapterFactoryTest, givenFailingEnumAdapters3WhenCreatingSnapshotThenFalseIsReturnedAndSnapshotIsEmpty) {
    for (uint32_t failingCall : {1u, 2u}) {
        failEnumAdapters3OnCall = 0u;
        EXPECT_TRUE(adapterFactory->createSnapshotOfAvailableAdapters());
        EXPECT_EQ(numAdapters, adapterFactory->getNumAdaptersInSnapshot());

        enumAdapters3CallCount = 0u;
        failEnumAdapters3OnCall = failingCall;

        EXPECT_FALSE(adapterFactory->createSnapshotOfAvailableAdapters());
        EXPECT_EQ(0u, adapterFactory->getNumAdaptersInSnapshot());
    }
}

TEST_F(KmtAdapterFactoryTest, whenCheckingSupportThenEnumAdapters3IsRequired) {
    EXPECT_TRUE(adapterFactory->isSupported());

    gdi.enumAdapters3.mFunc = nullptr;
    EXPECT_FALSE(adapterFactory->isSupported());
}

TEST_F(KmtAdapterFactoryTest, givenOrdinalOutOfSnapshotWhenGettingAdapterDescThenFalseIsReturned) {
    EXPECT_TRUE(adapterFactory->createSnapshotOfAvailableAdapters());

    AdapterFactory::AdapterDesc adapterDesc;
    EXPECT_FALSE(adapterFactory->getAdapterDesc(numAdapters, adapterDesc));
}

TEST_F(KmtAdapterFactoryTest, givenGdiWithoutEnumAdapters3WhenCreatingAdapterFactoryThenAdaptersFromDxCoreAreReported) {
    gdi.enumAdapters3.mFunc = nullptr;

    auto adapterFactory = AdapterFactory::create(gdi, ultDxCoreCreateAdapterFactory, nullptr);

    EXPECT_TRUE(adapterFactory->createSnapshotOfAvailableAdapters());
    ASSERT_EQ(1u, adapterFactory->getNumAdaptersInSnapshot());
    AdapterFactory::AdapterDesc adapterDesc;
    EXPECT_TRUE(adapterFactory->getAdapterDesc(0u, adapterDesc));
    EXPECT_EQ(UltAdapterLuid::dxCoreFactoryId, adapterDesc.luid.HighPart);
    EXPECT_EQ(UltAdapterLuid::adapterIndexBase, adapterDesc.luid.LowPart);
}

TEST_F(KmtAdapterFactoryTest, givenGdiWithEnumAdapters3WhenCreatingAdapterFactoryThenAdaptersFromEnumAdapters3AreReported) {
    auto adapterFactory = AdapterFactory::create(gdi, ultDxCoreCreateAdapterFactory, nullptr);

    EXPECT_TRUE(adapterFactory->createSnapshotOfAvailableAdapters());
    ASSERT_EQ(numAdapters, adapterFactory->getNumAdaptersInSnapshot());
    for (uint32_t i = 0; i < numAdapters; i++) {
        AdapterFactory::AdapterDesc adapterDesc;
        EXPECT_TRUE(adapterFactory->getAdapterDesc(i, adapterDesc));
        EXPECT_EQ(UltAdapterLuid::kmtFactoryId, adapterDesc.luid.HighPart);
        EXPECT_EQ(UltAdapterLuid::adapterIndexBase + i, adapterDesc.luid.LowPart);
        EXPECT_EQ(mockEnumeratedAdapterDeviceIdBase + i, adapterDesc.deviceId);
    }
}

} // namespace
