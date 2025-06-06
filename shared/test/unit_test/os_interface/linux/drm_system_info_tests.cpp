/*
 * Copyright (C) 2020-2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/os_interface/linux/ioctl_helper.h"
#include "shared/source/os_interface/linux/system_info.h"
#include "shared/source/os_interface/product_helper.h"
#include "shared/source/release_helper/release_helper.h"
#include "shared/test/common/helpers/debug_manager_state_restore.h"
#include "shared/test/common/helpers/default_hw_info.h"
#include "shared/test/common/helpers/gtest_helpers.h"
#include "shared/test/common/helpers/stream_capture.h"
#include "shared/test/common/libult/linux/drm_mock.h"
#include "shared/test/common/mocks/mock_execution_environment.h"
#include "shared/test/common/os_interface/linux/drm_mock_device_blob.h"
#include "shared/test/common/test_macros/test.h"

using namespace NEO;

TEST(DrmSystemInfoTest, whenQueryingSystemInfoThenSystemInfoIsNotCreatedAndIoctlsAreCalledOnce) {
    auto executionEnvironment = std::make_unique<MockExecutionEnvironment>();
    DrmMock drm(*executionEnvironment->rootDeviceEnvironments[0]);

    auto ioctlCount = drm.ioctlCount.total.load();
    EXPECT_FALSE(drm.querySystemInfo());

    EXPECT_EQ(nullptr, drm.getSystemInfo());
    EXPECT_EQ(1 + ioctlCount, drm.ioctlCount.total.load());
}

TEST(DrmSystemInfoTest, givenSystemInfoCreatedWhenQueryingSpecificAtrributesThenReturnZero) {
    std::vector<uint32_t> inputData{};
    SystemInfo systemInfo(inputData);

    EXPECT_EQ(0u, systemInfo.getMemoryType());
    EXPECT_EQ(0u, systemInfo.getMaxMemoryChannels());
    EXPECT_EQ(0u, systemInfo.getNumThreadsPerEu());
    EXPECT_EQ(0u, systemInfo.getMaxEuPerDualSubSlice());
    EXPECT_EQ(0u, systemInfo.getMaxSlicesSupported());
    EXPECT_EQ(0u, systemInfo.getMaxDualSubSlicesSupported());
    EXPECT_EQ(0u, systemInfo.getMaxRCS());
    EXPECT_EQ(0u, systemInfo.getMaxCCS());
    EXPECT_EQ(0u, systemInfo.getL3BankSizeInKb());
    EXPECT_EQ(0u, systemInfo.getSlmSizePerDss());
    EXPECT_EQ(0u, systemInfo.getCsrSizeInMb());
    EXPECT_EQ(0u, systemInfo.getNumRegions());
}

struct DrmMockToQuerySystemInfo : public DrmMock {
    DrmMockToQuerySystemInfo(RootDeviceEnvironment &rootDeviceEnvironment)
        : DrmMock(rootDeviceEnvironment) {}
    bool querySystemInfo() override {
        systemInfoQueried = true;
        return false;
    }
};
TEST(DrmSystemInfoTest, givenSetupHardwareInfoWhenQuerySystemInfoFalseThenSystemInfoIsNotCreated) {

    class MyMockIoctlHelper : public IoctlHelperPrelim20 {
      public:
        using IoctlHelperPrelim20::IoctlHelperPrelim20;
        void setupIpVersion() override {
        }
    };

    auto executionEnvironment = std::make_unique<MockExecutionEnvironment>();
    executionEnvironment->rootDeviceEnvironments[0]->initGmm();

    DrmMockToQuerySystemInfo drm(*executionEnvironment->rootDeviceEnvironments[0]);

    drm.ioctlHelper = std::make_unique<MyMockIoctlHelper>(drm);

    HardwareInfo hwInfo = *defaultHwInfo;
    auto setupHardwareInfo = [](HardwareInfo *, bool, const ReleaseHelper *) {};
    DeviceDescriptor device = {0, &hwInfo, setupHardwareInfo};

    StreamCapture capture;
    capture.captureStdout();
    ::testing::internal::CaptureStderr();

    DebugManagerStateRestore restorer;
    debugManager.flags.PrintDebugMessages.set(true);

    int ret = drm.setupHardwareInfo(&device, false);
    EXPECT_EQ(ret, 0);
    EXPECT_EQ(nullptr, drm.getSystemInfo());

    EXPECT_TRUE(isEmpty(capture.getCapturedStdout()));
    EXPECT_FALSE(isEmpty(::testing::internal::GetCapturedStderr()));
}

TEST(DrmSystemInfoTest, whenSetupHardwareInfoThenReleaseHelperContainsCorrectIpVersion) {

    class MyMockIoctlHelper : public IoctlHelperPrelim20 {
      public:
        using IoctlHelperPrelim20::IoctlHelperPrelim20;
        void setupIpVersion() override {
            drm.getRootDeviceEnvironment().getMutableHardwareInfo()->ipVersion.architecture = 12u;
            drm.getRootDeviceEnvironment().getMutableHardwareInfo()->ipVersion.release = 55u;
        }
    };

    auto executionEnvironment = std::make_unique<MockExecutionEnvironment>();
    executionEnvironment->rootDeviceEnvironments[0]->releaseHelper.reset(nullptr);
    executionEnvironment->rootDeviceEnvironments[0]->initGmm();
    DrmMockToQuerySystemInfo drm(*executionEnvironment->rootDeviceEnvironments[0]);
    drm.ioctlHelper = std::make_unique<MyMockIoctlHelper>(drm);
    HardwareInfo hwInfo = *defaultHwInfo;
    auto setupHardwareInfo = [](HardwareInfo *, bool, const ReleaseHelper *) {};
    DeviceDescriptor device = {0, &hwInfo, setupHardwareInfo};

    int ret = drm.setupHardwareInfo(&device, false);
    ASSERT_EQ(ret, 0);

    auto *releaseHelper = drm.getRootDeviceEnvironment().getReleaseHelper();

    if (releaseHelper == nullptr) {
        GTEST_SKIP();
    }
    class ReleaseHelperExpose : public ReleaseHelper {
      public:
        using ReleaseHelper::hardwareIpVersion;
    };

    ReleaseHelperExpose *exposedReleaseHelper = static_cast<ReleaseHelperExpose *>(releaseHelper);
    EXPECT_EQ(12u, exposedReleaseHelper->hardwareIpVersion.architecture);
    EXPECT_EQ(55u, exposedReleaseHelper->hardwareIpVersion.release);
}

TEST(DrmSystemInfoTest, whenQueryingSystemInfoTwiceThenSystemInfoIsCreatedOnlyOnce) {
    auto executionEnvironment = std::make_unique<MockExecutionEnvironment>();
    DrmMockEngine drm(*executionEnvironment->rootDeviceEnvironments[0]);

    EXPECT_TRUE(drm.querySystemInfo());

    auto systemInfo = drm.getSystemInfo();
    EXPECT_NE(nullptr, systemInfo);
    EXPECT_EQ(2u + drm.getBaseIoctlCalls(), drm.ioctlCallsCount);

    EXPECT_TRUE(drm.querySystemInfo());
    EXPECT_EQ(systemInfo, drm.getSystemInfo());
    EXPECT_EQ(2u + drm.getBaseIoctlCalls(), drm.ioctlCallsCount);
}

TEST(DrmSystemInfoTest, whenQueryingSystemInfoThenSystemInfoIsCreatedAndReturnsNonZeros) {
    auto executionEnvironment = std::make_unique<MockExecutionEnvironment>();
    DrmMockEngine drm(*executionEnvironment->rootDeviceEnvironments[0]);

    EXPECT_TRUE(drm.querySystemInfo());

    auto systemInfo = drm.getSystemInfo();
    EXPECT_NE(nullptr, systemInfo);

    EXPECT_NE(0u, systemInfo->getMaxMemoryChannels());
    EXPECT_NE(0u, systemInfo->getMemoryType());
    EXPECT_NE(0u, systemInfo->getMaxEuPerDualSubSlice());
    EXPECT_NE(0u, systemInfo->getMaxSlicesSupported());
    EXPECT_NE(0u, systemInfo->getMaxDualSubSlicesSupported());
    EXPECT_NE(0u, systemInfo->getMaxDualSubSlicesSupported());
    EXPECT_NE(0u, systemInfo->getMaxRCS());
    EXPECT_NE(0u, systemInfo->getMaxCCS());
    EXPECT_NE(0u, systemInfo->getL3BankSizeInKb());
    EXPECT_NE(0u, systemInfo->getSlmSizePerDss());
    EXPECT_NE(0u, systemInfo->getCsrSizeInMb());
    EXPECT_NE(0u, systemInfo->getNumRegions());

    EXPECT_EQ(2u + drm.getBaseIoctlCalls(), drm.ioctlCallsCount);
}

TEST(DrmSystemInfoTest, givenSystemInfoCreatedFromDeviceBlobWhenQueryingSpecificAtrributesThenReturnCorrectValues) {
    SystemInfo systemInfo(inputBlobData);

    EXPECT_EQ(0x0Au, systemInfo.getMaxMemoryChannels());
    EXPECT_EQ(0x0Bu, systemInfo.getMemoryType());
    EXPECT_EQ(0x03u, systemInfo.getMaxEuPerDualSubSlice());
    EXPECT_EQ(0x01u, systemInfo.getMaxSlicesSupported());
    EXPECT_EQ(0x04u, systemInfo.getMaxDualSubSlicesSupported());
    EXPECT_EQ(0x17u, systemInfo.getMaxRCS());
    EXPECT_EQ(0x18u, systemInfo.getMaxCCS());
    EXPECT_EQ(0x2Du, systemInfo.getL3BankSizeInKb());
    EXPECT_EQ(0x24u, systemInfo.getSlmSizePerDss());
    EXPECT_EQ(0x25u, systemInfo.getCsrSizeInMb());
    EXPECT_EQ(0x04u, systemInfo.getNumHbmStacksPerTile());
    EXPECT_EQ(0x08u, systemInfo.getNumChannlesPerHbmStack());
    EXPECT_EQ(0x02u, systemInfo.getNumRegions());
}

TEST(DrmSystemInfoTest, givenSystemInfoCreatedFromDeviceBlobAndDifferentMaxSubSlicesAndMaxDSSThenQueryReturnsTheMaxValue) {
    uint32_t hwBlob0[] = {NEO::DeviceBlobConstants::maxDualSubSlicesSupported, 1, 4, NEO::DeviceBlobConstants::maxSubSlicesSupported, 1, 8};
    std::vector<uint32_t> inputBlobData0(reinterpret_cast<uint32_t *>(hwBlob0), reinterpret_cast<uint32_t *>(ptrOffset(hwBlob0, sizeof(hwBlob0))));
    SystemInfo systemInfo0(inputBlobData0);
    EXPECT_EQ(8u, systemInfo0.getMaxDualSubSlicesSupported());

    uint32_t hwBlob1[] = {NEO::DeviceBlobConstants::maxDualSubSlicesSupported, 1, 16, NEO::DeviceBlobConstants::maxSubSlicesSupported, 1, 8};
    std::vector<uint32_t> inputBlobData1(reinterpret_cast<uint32_t *>(hwBlob1), reinterpret_cast<uint32_t *>(ptrOffset(hwBlob1, sizeof(hwBlob1))));
    SystemInfo systemInfo1(inputBlobData1);
    EXPECT_EQ(16u, systemInfo1.getMaxDualSubSlicesSupported());
}

TEST(DrmSystemInfoTest, givenSystemInfoCreatedFromDeviceBlobAndDifferentMaxEuPerSubSliceAndMaxEuPerDSSThenQueryReturnsTheMaxValue) {
    uint32_t hwBlob0[] = {NEO::DeviceBlobConstants::maxEuPerDualSubSlice, 1, 7, NEO::DeviceBlobConstants::maxEuPerSubSlice, 1, 8};
    std::vector<uint32_t> inputBlobData0(reinterpret_cast<uint32_t *>(hwBlob0), reinterpret_cast<uint32_t *>(ptrOffset(hwBlob0, sizeof(hwBlob0))));
    SystemInfo systemInfo0(inputBlobData0);
    EXPECT_EQ(8u, systemInfo0.getMaxEuPerDualSubSlice());

    uint32_t hwBlob1[] = {NEO::DeviceBlobConstants::maxEuPerDualSubSlice, 1, 8, NEO::DeviceBlobConstants::maxEuPerSubSlice, 1, 7};
    std::vector<uint32_t> inputBlobData1(reinterpret_cast<uint32_t *>(hwBlob1), reinterpret_cast<uint32_t *>(ptrOffset(hwBlob1, sizeof(hwBlob1))));
    SystemInfo systemInfo1(inputBlobData1);
    EXPECT_EQ(8u, systemInfo1.getMaxEuPerDualSubSlice());
}

TEST(DrmSystemInfoTest, givenSystemInfoCreatedFromDeviceBlobAndDifferentSlmSizePerDssAndSlmSizePerSsThenQueryReturnsTheMaxValue) {
    uint32_t hwBlob0[] = {NEO::DeviceBlobConstants::slmSizePerDss, 1, 7, NEO::DeviceBlobConstants::slmSizePerSs, 1, 8};
    std::vector<uint32_t> inputBlobData0(reinterpret_cast<uint32_t *>(hwBlob0), reinterpret_cast<uint32_t *>(ptrOffset(hwBlob0, sizeof(hwBlob0))));
    SystemInfo systemInfo0(inputBlobData0);
    EXPECT_EQ(8u, systemInfo0.getSlmSizePerDss());

    uint32_t hwBlob1[] = {NEO::DeviceBlobConstants::slmSizePerDss, 1, 8, NEO::DeviceBlobConstants::slmSizePerSs, 1, 7};
    std::vector<uint32_t> inputBlobData1(reinterpret_cast<uint32_t *>(hwBlob1), reinterpret_cast<uint32_t *>(ptrOffset(hwBlob1, sizeof(hwBlob1))));
    SystemInfo systemInfo1(inputBlobData1);
    EXPECT_EQ(8u, systemInfo1.getSlmSizePerDss());
}

TEST(DrmSystemInfoTest, givenSetupHardwareInfoWhenQuerySystemInfoFailsThenSystemInfoIsNotCreatedAndDebugMessageIsPrinted) {
    auto executionEnvironment = std::make_unique<MockExecutionEnvironment>();
    executionEnvironment->rootDeviceEnvironments[0]->initGmm();

    DrmMockEngine drm(*executionEnvironment->rootDeviceEnvironments[0]);
    drm.ioctlHelper = std::make_unique<IoctlHelperPrelim20>(drm);

    HardwareInfo hwInfo = *defaultHwInfo;
    auto setupHardwareInfo = [](HardwareInfo *, bool, const ReleaseHelper *) {};
    DeviceDescriptor device = {0, &hwInfo, setupHardwareInfo};

    StreamCapture capture;
    capture.captureStdout();
    ::testing::internal::CaptureStderr();
    DebugManagerStateRestore restorer;
    debugManager.flags.PrintDebugMessages.set(true);

    drm.failQueryDeviceBlob = true;

    int ret = drm.setupHardwareInfo(&device, false);
    debugManager.flags.PrintDebugMessages.set(false);
    EXPECT_EQ(ret, 0);
    EXPECT_EQ(nullptr, drm.getSystemInfo());

    EXPECT_TRUE(hasSubstr(capture.getCapturedStdout(), "INFO: System Info query failed!\n"));
    auto &productHelper = executionEnvironment->rootDeviceEnvironments[0]->getHelper<ProductHelper>();
    if (productHelper.isPlatformQuerySupported()) {
        EXPECT_TRUE(hasSubstr(::testing::internal::GetCapturedStderr(), "Size got from PRELIM_DRM_I915_QUERY_HW_IP_VERSION query does not match PrelimI915::prelim_drm_i915_query_hw_ip_version size\n"));
    } else {
        EXPECT_FALSE(::testing::internal::GetCapturedStderr().empty());
    }
}

TEST(DrmSystemInfoTest, givenSetupHardwareInfoWhenQuerySystemInfoSucceedsThenSystemInfoIsCreatedAndUsedToSetHardwareInfoAttributes) {
    auto executionEnvironment = std::make_unique<MockExecutionEnvironment>();
    executionEnvironment->rootDeviceEnvironments[0]->initGmm();

    DrmMockEngine drm(*executionEnvironment->rootDeviceEnvironments[0]);

    HardwareInfo hwInfo = *defaultHwInfo;

    hwInfo.capabilityTable.maxProgrammableSlmSize = 0x1234678u;

    auto setupHardwareInfo = [](HardwareInfo *, bool, const ReleaseHelper *) {};
    DeviceDescriptor device = {0, &hwInfo, setupHardwareInfo};

    int ret = drm.setupHardwareInfo(&device, false);
    EXPECT_EQ(ret, 0);
    EXPECT_NE(nullptr, drm.getSystemInfo());
    const auto &newHwInfo = *executionEnvironment->rootDeviceEnvironments[0]->getHardwareInfo();
    const auto &gtSystemInfo = newHwInfo.gtSystemInfo;

    EXPECT_GT(gtSystemInfo.MaxEuPerSubSlice, 0u);
    EXPECT_GT(gtSystemInfo.MaxSlicesSupported, 0u);
    EXPECT_GT(gtSystemInfo.MaxSubSlicesSupported, 0u);
    EXPECT_GT(gtSystemInfo.MaxDualSubSlicesSupported, 0u);
    EXPECT_GT(gtSystemInfo.MemoryType, 0u);
    EXPECT_EQ(gtSystemInfo.CsrSizeInMb, drm.getSystemInfo()->getCsrSizeInMb());
    EXPECT_EQ(gtSystemInfo.SLMSizeInKb, drm.getSystemInfo()->getSlmSizePerDss());
    EXPECT_EQ(newHwInfo.capabilityTable.maxProgrammableSlmSize, hwInfo.capabilityTable.maxProgrammableSlmSize);
    EXPECT_NE(newHwInfo.capabilityTable.maxProgrammableSlmSize, drm.getSystemInfo()->getSlmSizePerDss());
}

TEST(DrmSystemInfoTest, givenSetupHardwareInfoWhenQuerySystemInfoSucceedsThenSystemInfoIsCreatedAndHardwareInfoSetProperlyBasedOnBlobData) {
    auto executionEnvironment = std::make_unique<MockExecutionEnvironment>();
    executionEnvironment->rootDeviceEnvironments[0]->initGmm();

    DrmMockEngine drm(*executionEnvironment->rootDeviceEnvironments[0]);

    HardwareInfo hwInfo = *defaultHwInfo;

    hwInfo.gtSystemInfo.MaxSlicesSupported = 0u;
    hwInfo.gtSystemInfo.MaxSubSlicesSupported = 0u;
    hwInfo.gtSystemInfo.MaxEuPerSubSlice = 0u;
    drm.storedSVal = 0u;
    drm.storedSSVal = 1u;
    drm.storedEUVal = 1u;

    auto expectedMaxSubslicesSupported = dummyDeviceBlobData[5];
    auto expectedMaxEusPerSubsliceSupported = dummyDeviceBlobData[8];

    auto setupHardwareInfo = [](HardwareInfo *, bool, const ReleaseHelper *) {};
    DeviceDescriptor device = {0, &hwInfo, setupHardwareInfo};

    int ret = drm.setupHardwareInfo(&device, false);
    EXPECT_EQ(ret, 0);
    EXPECT_NE(nullptr, drm.getSystemInfo());
    const auto &gtSystemInfo = executionEnvironment->rootDeviceEnvironments[0]->getHardwareInfo()->gtSystemInfo;

    EXPECT_EQ(expectedMaxSubslicesSupported, gtSystemInfo.MaxSubSlicesSupported);
    EXPECT_NE(0u, gtSystemInfo.MaxSubSlicesSupported);

    EXPECT_EQ(expectedMaxEusPerSubsliceSupported, gtSystemInfo.MaxEuPerSubSlice);
    EXPECT_NE(0u, gtSystemInfo.MaxEuPerSubSlice);
}

TEST(DrmSystemInfoTest, givenSetupHardwareInfoWhenQuerySystemInfoSucceedsAndBlobHasZerosThenHardwareInfoDefaultValuesNotChanged) {
    auto executionEnvironment = std::make_unique<MockExecutionEnvironment>();
    executionEnvironment->rootDeviceEnvironments[0]->initGmm();

    DrmMockEngine drm(*executionEnvironment->rootDeviceEnvironments[0]);

    HardwareInfo hwInfo = *defaultHwInfo;

    auto setupHardwareInfo = [](HardwareInfo *, bool, const ReleaseHelper *) {};
    DeviceDescriptor device = {0, &hwInfo, setupHardwareInfo};

    drm.systemInfo.reset(new SystemInfo(inputBlobDataZeros));
    drm.systemInfoQueried = true;
    int ret = drm.setupHardwareInfo(&device, false);
    EXPECT_EQ(ret, 0);
    EXPECT_NE(nullptr, drm.getSystemInfo());
    const auto &newHwInfo = *executionEnvironment->rootDeviceEnvironments[0]->getHardwareInfo();

    EXPECT_EQ(newHwInfo.featureTable.regionCount, 1u);
}

TEST(DrmSystemInfoTest, givenZeroBankCountWhenCreatingSystemInfoThenUseDualSubslicesToCalculateL3Size) {
    auto executionEnvironment = std::make_unique<MockExecutionEnvironment>();
    executionEnvironment->rootDeviceEnvironments[0]->initGmm();

    DrmMockEngine drm(*executionEnvironment->rootDeviceEnvironments[0]);

    HardwareInfo hwInfo = *defaultHwInfo;
    hwInfo.gtSystemInfo.L3BankCount = 0;

    auto setupHardwareInfo = [](HardwareInfo *, bool, const ReleaseHelper *) {};
    DeviceDescriptor device = {0, &hwInfo, setupHardwareInfo};

    int ret = drm.setupHardwareInfo(&device, false);
    EXPECT_EQ(ret, 0);
    EXPECT_NE(nullptr, drm.getSystemInfo());
    const auto &gtSystemInfo = executionEnvironment->rootDeviceEnvironments[0]->getHardwareInfo()->gtSystemInfo;

    EXPECT_EQ(gtSystemInfo.MaxDualSubSlicesSupported, gtSystemInfo.L3BankCount);

    uint64_t expectedL3Size = gtSystemInfo.MaxDualSubSlicesSupported * drm.getSystemInfo()->getL3BankSizeInKb();
    uint64_t calculatedL3Size = gtSystemInfo.L3CacheSizeInKb;

    EXPECT_EQ(expectedL3Size, calculatedL3Size);
}

TEST(DrmSystemInfoTest, givenNonZeroBankCountWhenCreatingSystemInfoThenUseDualSubslicesToCalculateL3Size) {
    auto executionEnvironment = std::make_unique<MockExecutionEnvironment>();
    executionEnvironment->rootDeviceEnvironments[0]->initGmm();

    DrmMockEngine drm(*executionEnvironment->rootDeviceEnvironments[0]);

    HardwareInfo hwInfo = *defaultHwInfo;
    hwInfo.gtSystemInfo.L3BankCount = 5;

    auto setupHardwareInfo = [](HardwareInfo *, bool, const ReleaseHelper *) {};
    DeviceDescriptor device = {0, &hwInfo, setupHardwareInfo};

    int ret = drm.setupHardwareInfo(&device, false);
    EXPECT_EQ(ret, 0);
    EXPECT_NE(nullptr, drm.getSystemInfo());
    const auto &gtSystemInfo = executionEnvironment->rootDeviceEnvironments[0]->getHardwareInfo()->gtSystemInfo;

    EXPECT_NE(0u, gtSystemInfo.L3BankCount);

    uint64_t expectedL3Size = gtSystemInfo.L3BankCount * drm.getSystemInfo()->getL3BankSizeInKb();
    uint64_t calculatedL3Size = gtSystemInfo.L3CacheSizeInKb;

    EXPECT_EQ(expectedL3Size, calculatedL3Size);
}

TEST(DrmSystemInfoTest, givenNumL3BanksSetInTopoologyDataWhenCreatingSystemInfoThenRespectThatNumberOfL3Banks) {
    auto executionEnvironment = std::make_unique<MockExecutionEnvironment>();
    executionEnvironment->rootDeviceEnvironments[0]->initGmm();
    class MyMockIoctlHelper : public IoctlHelperPrelim20 {
      public:
        using IoctlHelperPrelim20::IoctlHelperPrelim20;

        bool getTopologyDataAndMap(const HardwareInfo &hwInfo, DrmQueryTopologyData &topologyData, TopologyMap &topologyMap) override {
            IoctlHelperPrelim20::getTopologyDataAndMap(hwInfo, topologyData, topologyMap);
            topologyData.numL3Banks = 7;
            return true;
        }
    };

    DrmMockEngine drm(*executionEnvironment->rootDeviceEnvironments[0]);
    drm.ioctlHelper = std::make_unique<MyMockIoctlHelper>(drm);

    HardwareInfo hwInfo = *defaultHwInfo;
    hwInfo.gtSystemInfo.L3BankCount = 5;

    uint32_t expectedNumOfL3Banks = 7;

    auto setupHardwareInfo = [](HardwareInfo *, bool, const ReleaseHelper *) {};
    DeviceDescriptor device = {0, &hwInfo, setupHardwareInfo};

    int ret = drm.setupHardwareInfo(&device, false);
    EXPECT_EQ(ret, 0);
    EXPECT_NE(nullptr, drm.getSystemInfo());
    const auto &gtSystemInfo = executionEnvironment->rootDeviceEnvironments[0]->getHardwareInfo()->gtSystemInfo;

    EXPECT_EQ(expectedNumOfL3Banks, gtSystemInfo.L3BankCount);

    uint64_t expectedL3Size = gtSystemInfo.L3BankCount * drm.getSystemInfo()->getL3BankSizeInKb();
    uint64_t calculatedL3Size = gtSystemInfo.L3CacheSizeInKb;

    EXPECT_EQ(expectedL3Size, calculatedL3Size);
}

TEST(DrmSystemInfoTest, givenHardwareInfoWithoutEuCountWhenQuerySystemInfoSucceedsThenEuCountIsSetBasedOnMaxEuPerSubSlice) {
    auto executionEnvironment = std::make_unique<MockExecutionEnvironment>();
    executionEnvironment->rootDeviceEnvironments[0]->initGmm();
    HardwareInfo &hwInfo = *executionEnvironment->rootDeviceEnvironments[0]->getMutableHardwareInfo();

    hwInfo.gtSystemInfo.EUCount = 0u;
    hwInfo.gtSystemInfo.SubSliceCount = 2u;
    hwInfo.gtSystemInfo.DualSubSliceCount = 2u;

    DrmMockEngine drm(*executionEnvironment->rootDeviceEnvironments[0]);

    drm.storedEUVal = 0;
    drm.failRetTopology = true;

    auto setupHardwareInfo = [](HardwareInfo *, bool, const ReleaseHelper *) {};
    DeviceDescriptor device = {0, &hwInfo, setupHardwareInfo};

    int ret = drm.setupHardwareInfo(&device, false);
    EXPECT_EQ(ret, 0);
    EXPECT_NE(nullptr, drm.getSystemInfo());
    const auto &newHwInfo = *executionEnvironment->rootDeviceEnvironments[0]->getHardwareInfo();
    const auto &gtSystemInfo = newHwInfo.gtSystemInfo;

    EXPECT_NE(0u, gtSystemInfo.EUCount);
    EXPECT_EQ(hwInfo.gtSystemInfo.SubSliceCount * drm.getSystemInfo()->getMaxEuPerDualSubSlice(), gtSystemInfo.EUCount);
}

TEST(DrmSystemInfoTest, givenHardwareInfoWithoutEuCountWhenQuerySystemInfoFailsThenFailureIsReturned) {
    auto executionEnvironment = std::make_unique<MockExecutionEnvironment>();
    executionEnvironment->rootDeviceEnvironments[0]->initGmm();
    DrmMockEngine drm(*executionEnvironment->rootDeviceEnvironments[0]);

    HardwareInfo &hwInfo = *executionEnvironment->rootDeviceEnvironments[0]->getMutableHardwareInfo();

    hwInfo.gtSystemInfo.EUCount = 0u;
    hwInfo.gtSystemInfo.SubSliceCount = 2u;
    hwInfo.gtSystemInfo.DualSubSliceCount = 2u;
    hwInfo.gtSystemInfo.MaxEuPerSubSlice = 0u;

    drm.storedEUVal = 0;
    drm.failRetTopology = true;
    drm.failQueryDeviceBlob = true;

    auto setupHardwareInfo = [](HardwareInfo *, bool, const ReleaseHelper *) {};
    DeviceDescriptor device = {0, &hwInfo, setupHardwareInfo};

    int ret = drm.setupHardwareInfo(&device, false);
    EXPECT_EQ(ret, -1);
    EXPECT_EQ(nullptr, drm.getSystemInfo());
}

TEST(DrmSystemInfoTest, givenTopologyWithMoreEuPerDssThanInDeviceBlobWhenSetupHardwareInfoThenDeviceBlobValueLimitsEuPerDssCount) {
    auto executionEnvironment = std::make_unique<MockExecutionEnvironment>();
    executionEnvironment->rootDeviceEnvironments[0]->initGmm();
    HardwareInfo &hwInfo = *executionEnvironment->rootDeviceEnvironments[0]->getMutableHardwareInfo();

    DrmMockEngine drm(*executionEnvironment->rootDeviceEnvironments[0]);

    drm.storedSSVal = 2;
    drm.storedEUVal = 200;

    auto setupHardwareInfo = [](HardwareInfo *, bool, const ReleaseHelper *) {};
    DeviceDescriptor device = {0, &hwInfo, setupHardwareInfo};

    int ret = drm.setupHardwareInfo(&device, false);
    EXPECT_EQ(ret, 0);
    EXPECT_NE(nullptr, drm.getSystemInfo());
    const auto &newHwInfo = *executionEnvironment->rootDeviceEnvironments[0]->getHardwareInfo();
    const auto &gtSystemInfo = newHwInfo.gtSystemInfo;

    EXPECT_NE(static_cast<uint32_t>(drm.storedEUVal), gtSystemInfo.EUCount);
    EXPECT_EQ(hwInfo.gtSystemInfo.SubSliceCount * drm.getSystemInfo()->getMaxEuPerDualSubSlice(), gtSystemInfo.EUCount);
    EXPECT_EQ(hwInfo.gtSystemInfo.EUCount * drm.getSystemInfo()->getNumThreadsPerEu(), gtSystemInfo.ThreadCount);
}

TEST(DrmSystemInfoTest, givenOverrideNumThreadsPerEuSetWhenSetupHardwareInfoThenCorrectThreadCountIsSet) {
    DebugManagerStateRestore restorer;

    auto executionEnvironment = std::make_unique<MockExecutionEnvironment>();
    auto &hwInfo = *executionEnvironment->rootDeviceEnvironments[0]->getMutableHardwareInfo();
    auto setupHardwareInfo = [](HardwareInfo *, bool, const ReleaseHelper *) {};
    DeviceDescriptor device = {0, &hwInfo, setupHardwareInfo};

    uint32_t dummyBlobThreadCount = 90;
    uint32_t dummyBlobEuCount = 6;
    {
        DrmMockEngine drm(*executionEnvironment->rootDeviceEnvironments[0]);
        drm.setupHardwareInfo(&device, false);
        EXPECT_EQ(hwInfo.gtSystemInfo.ThreadCount, dummyBlobThreadCount);
    }
    {
        debugManager.flags.OverrideNumThreadsPerEu.set(7);
        DrmMockEngine drm(*executionEnvironment->rootDeviceEnvironments[0]);
        drm.setupHardwareInfo(&device, false);
        EXPECT_EQ(hwInfo.gtSystemInfo.ThreadCount, dummyBlobEuCount * 7);
    }
    {
        debugManager.flags.OverrideNumThreadsPerEu.set(8);
        DrmMockEngine drm(*executionEnvironment->rootDeviceEnvironments[0]);
        drm.setupHardwareInfo(&device, false);
        EXPECT_EQ(hwInfo.gtSystemInfo.ThreadCount, dummyBlobEuCount * 8);
    }
    {
        debugManager.flags.OverrideNumThreadsPerEu.set(10);
        DrmMockEngine drm(*executionEnvironment->rootDeviceEnvironments[0]);
        drm.setupHardwareInfo(&device, false);
        EXPECT_EQ(hwInfo.gtSystemInfo.ThreadCount, dummyBlobEuCount * 10);
    }
}
