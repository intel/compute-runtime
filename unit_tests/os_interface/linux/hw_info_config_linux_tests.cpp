/*
 * Copyright (C) 2018-2019 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "unit_tests/os_interface/linux/hw_info_config_linux_tests.h"

#include "runtime/helpers/hw_helper.h"
#include "runtime/helpers/options.h"
#include "runtime/os_interface/linux/os_interface.h"

#include <cstring>

namespace NEO {

constexpr uint32_t hwConfigTestMidThreadBit = 1 << 8;
constexpr uint32_t hwConfigTestThreadGroupBit = 1 << 9;
constexpr uint32_t hwConfigTestMidBatchBit = 1 << 10;

template <>
int HwInfoConfigHw<IGFX_UNKNOWN>::configureHardwareCustom(HardwareInfo *hwInfo, OSInterface *osIface) {
    FeatureTable *featureTable = &hwInfo->featureTable;

    if (hwInfo->platform.usDeviceID == 30) {
        GT_SYSTEM_INFO *gtSystemInfo = &hwInfo->gtSystemInfo;
        gtSystemInfo->EdramSizeInKb = 128 * 1000;
    }
    if (hwInfo->platform.usDeviceID & hwConfigTestMidThreadBit) {
        featureTable->ftrGpGpuMidThreadLevelPreempt = 1;
    }
    if (hwInfo->platform.usDeviceID & hwConfigTestThreadGroupBit) {
        featureTable->ftrGpGpuThreadGroupLevelPreempt = 1;
    }
    if (hwInfo->platform.usDeviceID & hwConfigTestMidBatchBit) {
        featureTable->ftrGpGpuMidBatchPreempt = 1;
    }
    return (hwInfo->platform.usDeviceID == 10) ? -1 : 0;
}

template <>
void HwInfoConfigHw<IGFX_UNKNOWN>::adjustPlatformForProductFamily(HardwareInfo *hwInfo) {
}

template <>
cl_unified_shared_memory_capabilities_intel HwInfoConfigHw<IGFX_UNKNOWN>::getHostMemCapabilities() {
    return 0;
}

template <>
cl_unified_shared_memory_capabilities_intel HwInfoConfigHw<IGFX_UNKNOWN>::getDeviceMemCapabilities() {
    return 0;
}

template <>
cl_unified_shared_memory_capabilities_intel HwInfoConfigHw<IGFX_UNKNOWN>::getSingleDeviceSharedMemCapabilities() {
    return 0;
}

template <>
cl_unified_shared_memory_capabilities_intel HwInfoConfigHw<IGFX_UNKNOWN>::getCrossDeviceSharedMemCapabilities() {
    return 0;
}

template <>
cl_unified_shared_memory_capabilities_intel HwInfoConfigHw<IGFX_UNKNOWN>::getSharedSystemMemCapabilities() {
    return 0;
}

} // namespace NEO

struct DummyHwConfig : HwInfoConfigHw<IGFX_UNKNOWN> {
};

using namespace NEO;
using namespace std;

void mockCpuidex(int *cpuInfo, int functionId, int subfunctionId);

void HwInfoConfigTestLinux::SetUp() {
    HwInfoConfigTest::SetUp();

    osInterface = new OSInterface();
    drm = new DrmMock();
    osInterface->get()->setDrm(static_cast<Drm *>(drm));

    drm->StoredDeviceID = pInHwInfo.platform.usDeviceID;
    drm->StoredDeviceRevID = 0;
    drm->StoredEUVal = pInHwInfo.gtSystemInfo.EUCount;
    drm->StoredSSVal = pInHwInfo.gtSystemInfo.SubSliceCount;

    rt_cpuidex_func = CpuInfo::cpuidexFunc;
    CpuInfo::cpuidexFunc = mockCpuidex;
}

void HwInfoConfigTestLinux::TearDown() {
    CpuInfo::cpuidexFunc = rt_cpuidex_func;

    delete drm;
    delete osInterface;

    HwInfoConfigTest::TearDown();
}

void mockCpuidex(int *cpuInfo, int functionId, int subfunctionId) {
    if (subfunctionId == 0) {
        cpuInfo[0] = 0x7F;
    }
    if (subfunctionId == 1) {
        cpuInfo[0] = 0x1F;
    }
    if (subfunctionId == 2) {
        cpuInfo[0] = 0;
    }
}

struct HwInfoConfigTestLinuxDummy : HwInfoConfigTestLinux {
    void SetUp() override {
        HwInfoConfigTestLinux::SetUp();

        drm->StoredDeviceID = 1;
        drm->setGtType(GTTYPE_GT0);
        testPlatform->eRenderCoreFamily = platformDevices[0]->platform.eRenderCoreFamily;
    }

    void TearDown() override {
        HwInfoConfigTestLinux::TearDown();
    }

    DummyHwConfig hwConfig;
};

TEST_F(HwInfoConfigTestLinuxDummy, dummyConfig) {
    int ret = hwConfig.configureHwInfo(&pInHwInfo, &outHwInfo, osInterface);
    EXPECT_EQ(0, ret);
}

GTTYPE GtTypes[] = {
    GTTYPE_GT1, GTTYPE_GT2, GTTYPE_GT1_5, GTTYPE_GT2_5, GTTYPE_GT3, GTTYPE_GT4, GTTYPE_GTA, GTTYPE_GTC, GTTYPE_GTX};

TEST_F(HwInfoConfigTestLinuxDummy, dummyConfigGtTypes) {
    int ret = hwConfig.configureHwInfo(&pInHwInfo, &outHwInfo, osInterface);
    EXPECT_EQ(0, ret);

    EXPECT_EQ(GTTYPE_GT0, outHwInfo.platform.eGTType);
    EXPECT_EQ(0u, outHwInfo.featureTable.ftrGT1);
    EXPECT_EQ(0u, outHwInfo.featureTable.ftrGT1_5);
    EXPECT_EQ(0u, outHwInfo.featureTable.ftrGT2);
    EXPECT_EQ(0u, outHwInfo.featureTable.ftrGT2_5);
    EXPECT_EQ(0u, outHwInfo.featureTable.ftrGT3);
    EXPECT_EQ(0u, outHwInfo.featureTable.ftrGT4);
    EXPECT_EQ(0u, outHwInfo.featureTable.ftrGTA);
    EXPECT_EQ(0u, outHwInfo.featureTable.ftrGTC);
    EXPECT_EQ(0u, outHwInfo.featureTable.ftrGTX);

    size_t arrSize = sizeof(GtTypes) / sizeof(GTTYPE);
    uint32_t FtrSum = 0;
    for (uint32_t i = 0; i < arrSize; i++) {
        drm->setGtType(GtTypes[i]);
        ret = hwConfig.configureHwInfo(&pInHwInfo, &outHwInfo, osInterface);
        EXPECT_EQ(0, ret);
        EXPECT_EQ(GtTypes[i], outHwInfo.platform.eGTType);
        bool FtrPresent = (outHwInfo.featureTable.ftrGT1 ||
                           outHwInfo.featureTable.ftrGT1_5 ||
                           outHwInfo.featureTable.ftrGT2 ||
                           outHwInfo.featureTable.ftrGT2_5 ||
                           outHwInfo.featureTable.ftrGT3 ||
                           outHwInfo.featureTable.ftrGT4 ||
                           outHwInfo.featureTable.ftrGTA ||
                           outHwInfo.featureTable.ftrGTC ||
                           outHwInfo.featureTable.ftrGTX);
        EXPECT_TRUE(FtrPresent);
        FtrSum += (outHwInfo.featureTable.ftrGT1 +
                   outHwInfo.featureTable.ftrGT1_5 +
                   outHwInfo.featureTable.ftrGT2 +
                   outHwInfo.featureTable.ftrGT2_5 +
                   outHwInfo.featureTable.ftrGT3 +
                   outHwInfo.featureTable.ftrGT4 +
                   outHwInfo.featureTable.ftrGTA +
                   outHwInfo.featureTable.ftrGTC +
                   outHwInfo.featureTable.ftrGTX);
    }
    EXPECT_EQ(arrSize, FtrSum);
}

TEST_F(HwInfoConfigTestLinuxDummy, dummyConfigEdramDetection) {
    drm->StoredDeviceID = 30;
    int ret = hwConfig.configureHwInfo(&pInHwInfo, &outHwInfo, osInterface);
    EXPECT_EQ(0, ret);
    EXPECT_EQ(1u, outHwInfo.featureTable.ftrEDram);
}

TEST_F(HwInfoConfigTestLinuxDummy, givenEnabledPlatformCoherencyWhenConfiguringHwInfoThenIgnoreAndSetAsDisabled) {
    drm->StoredDeviceID = 21;
    int ret = hwConfig.configureHwInfo(&pInHwInfo, &outHwInfo, osInterface);
    EXPECT_EQ(0, ret);
    EXPECT_FALSE(outHwInfo.capabilityTable.ftrSupportsCoherency);
}

TEST_F(HwInfoConfigTestLinuxDummy, givenDisabledPlatformCoherencyWhenConfiguringHwInfoThenSetValidCapability) {
    drm->StoredDeviceID = 20;
    int ret = hwConfig.configureHwInfo(&pInHwInfo, &outHwInfo, osInterface);
    EXPECT_EQ(0, ret);
    EXPECT_FALSE(outHwInfo.capabilityTable.ftrSupportsCoherency);
}

TEST_F(HwInfoConfigTestLinuxDummy, dummyNegativeUnknownGtType) {
    drm->setGtType(GTTYPE_UNDEFINED);

    int ret = hwConfig.configureHwInfo(&pInHwInfo, &outHwInfo, osInterface);
    EXPECT_EQ(-1, ret);
}

TEST_F(HwInfoConfigTestLinuxDummy, dummyNegativeUnknownDevId) {
    drm->StoredDeviceID = 0;

    int ret = hwConfig.configureHwInfo(&pInHwInfo, &outHwInfo, osInterface);
    EXPECT_EQ(-1, ret);
}

TEST_F(HwInfoConfigTestLinuxDummy, dummyNegativeFailGetDevId) {
    drm->StoredRetValForDeviceID = -2;

    int ret = hwConfig.configureHwInfo(&pInHwInfo, &outHwInfo, osInterface);
    EXPECT_EQ(-2, ret);
}

TEST_F(HwInfoConfigTestLinuxDummy, dummydummyNegativeFailGetDevRevId) {
    drm->StoredRetValForDeviceRevID = -3;

    int ret = hwConfig.configureHwInfo(&pInHwInfo, &outHwInfo, osInterface);
    EXPECT_EQ(-3, ret);
}

TEST_F(HwInfoConfigTestLinuxDummy, dummydummyNegativeFailGetEuCount) {
    drm->StoredRetValForEUVal = -4;

    int ret = hwConfig.configureHwInfo(&pInHwInfo, &outHwInfo, osInterface);
    EXPECT_EQ(-4, ret);
}

TEST_F(HwInfoConfigTestLinuxDummy, dummydummyNegativeFailGetSsCount) {
    drm->StoredRetValForSSVal = -5;

    int ret = hwConfig.configureHwInfo(&pInHwInfo, &outHwInfo, osInterface);
    EXPECT_EQ(-5, ret);
}

TEST_F(HwInfoConfigTestLinuxDummy, dummyNegativeFailingConfigureCustom) {
    drm->StoredDeviceID = 10;

    int ret = hwConfig.configureHwInfo(&pInHwInfo, &outHwInfo, osInterface);
    EXPECT_EQ(-1, ret);
}

TEST_F(HwInfoConfigTestLinuxDummy, dummyNegativeUnknownDeviceId) {
    drm->StoredDeviceID = 0;
    drm->setGtType(GTTYPE_GT1);

    auto hwConfig = DummyHwConfig{};
    int ret = hwConfig.configureHwInfo(&pInHwInfo, &outHwInfo, osInterface);
    EXPECT_EQ(-1, ret);
}

TEST_F(HwInfoConfigTestLinuxDummy, dummyConfigPreemptionDrmEnabledMidThreadOn) {
    pInHwInfo.capabilityTable.defaultPreemptionMode = PreemptionMode::MidThread;
    drm->StoredPreemptionSupport =
        I915_SCHEDULER_CAP_ENABLED |
        I915_SCHEDULER_CAP_PRIORITY |
        I915_SCHEDULER_CAP_PREEMPTION;
    drm->StoredDeviceID = hwConfigTestMidThreadBit;
    int ret = hwConfig.configureHwInfo(&pInHwInfo, &outHwInfo, osInterface);
    EXPECT_EQ(0, ret);
    EXPECT_EQ(PreemptionMode::MidThread, outHwInfo.capabilityTable.defaultPreemptionMode);
    EXPECT_TRUE(drm->isPreemptionSupported());
}

TEST_F(HwInfoConfigTestLinuxDummy, dummyConfigPreemptionDrmEnabledThreadGroupOn) {
    pInHwInfo.capabilityTable.defaultPreemptionMode = PreemptionMode::MidThread;
    drm->StoredPreemptionSupport =
        I915_SCHEDULER_CAP_ENABLED |
        I915_SCHEDULER_CAP_PRIORITY |
        I915_SCHEDULER_CAP_PREEMPTION;
    drm->StoredDeviceID = hwConfigTestThreadGroupBit;
    int ret = hwConfig.configureHwInfo(&pInHwInfo, &outHwInfo, osInterface);
    EXPECT_EQ(0, ret);
    EXPECT_EQ(PreemptionMode::ThreadGroup, outHwInfo.capabilityTable.defaultPreemptionMode);
    EXPECT_TRUE(drm->isPreemptionSupported());
}

TEST_F(HwInfoConfigTestLinuxDummy, dummyConfigPreemptionDrmEnabledMidBatchOn) {
    pInHwInfo.capabilityTable.defaultPreemptionMode = PreemptionMode::MidThread;
    drm->StoredPreemptionSupport =
        I915_SCHEDULER_CAP_ENABLED |
        I915_SCHEDULER_CAP_PRIORITY |
        I915_SCHEDULER_CAP_PREEMPTION;
    drm->StoredDeviceID = hwConfigTestMidBatchBit;
    int ret = hwConfig.configureHwInfo(&pInHwInfo, &outHwInfo, osInterface);
    EXPECT_EQ(0, ret);
    EXPECT_EQ(PreemptionMode::MidBatch, outHwInfo.capabilityTable.defaultPreemptionMode);
    EXPECT_TRUE(drm->isPreemptionSupported());
}

TEST_F(HwInfoConfigTestLinuxDummy, dummyConfigPreemptionDrmEnabledNoPreemption) {
    pInHwInfo.capabilityTable.defaultPreemptionMode = PreemptionMode::MidThread;
    drm->StoredPreemptionSupport =
        I915_SCHEDULER_CAP_ENABLED |
        I915_SCHEDULER_CAP_PRIORITY |
        I915_SCHEDULER_CAP_PREEMPTION;
    drm->StoredDeviceID = 1;
    int ret = hwConfig.configureHwInfo(&pInHwInfo, &outHwInfo, osInterface);
    EXPECT_EQ(0, ret);
    EXPECT_EQ(PreemptionMode::Disabled, outHwInfo.capabilityTable.defaultPreemptionMode);
    EXPECT_TRUE(drm->isPreemptionSupported());
}

TEST_F(HwInfoConfigTestLinuxDummy, dummyConfigPreemptionDrmDisabledAllPreemption) {
    pInHwInfo.capabilityTable.defaultPreemptionMode = PreemptionMode::MidThread;
    drm->StoredPreemptionSupport = 0;
    drm->StoredDeviceID = hwConfigTestMidThreadBit | hwConfigTestThreadGroupBit | hwConfigTestMidBatchBit;
    int ret = hwConfig.configureHwInfo(&pInHwInfo, &outHwInfo, osInterface);
    EXPECT_EQ(0, ret);
    EXPECT_EQ(PreemptionMode::Disabled, outHwInfo.capabilityTable.defaultPreemptionMode);
    EXPECT_FALSE(drm->isPreemptionSupported());
}

TEST_F(HwInfoConfigTestLinuxDummy, dummyConfigPreemptionDrmEnabledAllPreemptionDriverThreadGroup) {
    pInHwInfo.capabilityTable.defaultPreemptionMode = PreemptionMode::ThreadGroup;
    drm->StoredPreemptionSupport =
        I915_SCHEDULER_CAP_ENABLED |
        I915_SCHEDULER_CAP_PRIORITY |
        I915_SCHEDULER_CAP_PREEMPTION;
    drm->StoredDeviceID = hwConfigTestMidThreadBit | hwConfigTestThreadGroupBit | hwConfigTestMidBatchBit;
    int ret = hwConfig.configureHwInfo(&pInHwInfo, &outHwInfo, osInterface);
    EXPECT_EQ(0, ret);
    EXPECT_EQ(PreemptionMode::ThreadGroup, outHwInfo.capabilityTable.defaultPreemptionMode);
    EXPECT_TRUE(drm->isPreemptionSupported());
}

TEST_F(HwInfoConfigTestLinuxDummy, dummyConfigPreemptionDrmEnabledAllPreemptionDriverMidBatch) {
    pInHwInfo.capabilityTable.defaultPreemptionMode = PreemptionMode::MidBatch;
    drm->StoredPreemptionSupport =
        I915_SCHEDULER_CAP_ENABLED |
        I915_SCHEDULER_CAP_PRIORITY |
        I915_SCHEDULER_CAP_PREEMPTION;
    drm->StoredDeviceID = hwConfigTestMidThreadBit | hwConfigTestThreadGroupBit | hwConfigTestMidBatchBit;
    int ret = hwConfig.configureHwInfo(&pInHwInfo, &outHwInfo, osInterface);
    EXPECT_EQ(0, ret);
    EXPECT_EQ(PreemptionMode::MidBatch, outHwInfo.capabilityTable.defaultPreemptionMode);
    EXPECT_TRUE(drm->isPreemptionSupported());
}

TEST_F(HwInfoConfigTestLinuxDummy, dummyConfigPreemptionDrmEnabledAllPreemptionDriverDisabled) {
    pInHwInfo.capabilityTable.defaultPreemptionMode = PreemptionMode::Disabled;
    drm->StoredPreemptionSupport =
        I915_SCHEDULER_CAP_ENABLED |
        I915_SCHEDULER_CAP_PRIORITY |
        I915_SCHEDULER_CAP_PREEMPTION;
    drm->StoredDeviceID = hwConfigTestMidThreadBit | hwConfigTestThreadGroupBit | hwConfigTestMidBatchBit;
    int ret = hwConfig.configureHwInfo(&pInHwInfo, &outHwInfo, osInterface);
    EXPECT_EQ(0, ret);
    EXPECT_EQ(PreemptionMode::Disabled, outHwInfo.capabilityTable.defaultPreemptionMode);
    EXPECT_TRUE(drm->isPreemptionSupported());
}

TEST_F(HwInfoConfigTestLinuxDummy, givenPlatformEnabledFtrCompressionWhenInitializingThenForceDisable) {
    pInHwInfo.capabilityTable.ftrRenderCompressedBuffers = true;
    pInHwInfo.capabilityTable.ftrRenderCompressedImages = true;
    int ret = hwConfig.configureHwInfo(&pInHwInfo, &outHwInfo, osInterface);
    EXPECT_EQ(0, ret);
    EXPECT_FALSE(outHwInfo.capabilityTable.ftrRenderCompressedBuffers);
    EXPECT_FALSE(outHwInfo.capabilityTable.ftrRenderCompressedImages);
}

TEST_F(HwInfoConfigTestLinuxDummy, givenPointerToHwInfoWhenConfigureHwInfoCalledThenRequiedSurfaceSizeIsSettedProperly) {
    EXPECT_EQ(MemoryConstants::pageSize, pInHwInfo.capabilityTable.requiredPreemptionSurfaceSize);
    int ret = hwConfig.configureHwInfo(&pInHwInfo, &outHwInfo, osInterface);
    EXPECT_EQ(0, ret);
    EXPECT_EQ(outHwInfo.gtSystemInfo.CsrSizeInMb * MemoryConstants::megaByte, outHwInfo.capabilityTable.requiredPreemptionSurfaceSize);
}

TEST_F(HwInfoConfigTestLinuxDummy, givenInstrumentationForHardwareIsEnabledOrDisabledWhenConfiguringHwInfoThenOverrideItUsingHaveInstrumentation) {
    int ret;

    pInHwInfo.capabilityTable.instrumentationEnabled = false;
    ret = hwConfig.configureHwInfo(&pInHwInfo, &outHwInfo, osInterface);
    ASSERT_EQ(0, ret);
    EXPECT_FALSE(outHwInfo.capabilityTable.instrumentationEnabled);

    pInHwInfo.capabilityTable.instrumentationEnabled = true;
    ret = hwConfig.configureHwInfo(&pInHwInfo, &outHwInfo, osInterface);
    ASSERT_EQ(0, ret);
    EXPECT_TRUE(outHwInfo.capabilityTable.instrumentationEnabled == haveInstrumentation);
}