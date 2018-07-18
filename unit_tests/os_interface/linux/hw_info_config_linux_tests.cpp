/*
 * Copyright (c) 2018, Intel Corporation
 *
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the "Software"),
 * to deal in the Software without restriction, including without limitation
 * the rights to use, copy, modify, merge, publish, distribute, sublicense,
 * and/or sell copies of the Software, and to permit persons to whom the
 * Software is furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included
 * in all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS
 * OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL
 * THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR
 * OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE,
 * ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR
 * OTHER DEALINGS IN THE SOFTWARE.
 */

#include "runtime/helpers/hw_helper.h"
#include "runtime/helpers/options.h"
#include "runtime/os_interface/linux/os_interface.h"

#include "unit_tests/libult/mock_gfx_family.h"
#include "unit_tests/os_interface/linux/hw_info_config_linux_tests.h"

#include <cstring>

namespace OCLRT {

constexpr uint32_t hwConfigTestMidThreadBit = 1 << 8;
constexpr uint32_t hwConfigTestThreadGroupBit = 1 << 9;
constexpr uint32_t hwConfigTestMidBatchBit = 1 << 10;

template <>
int HwInfoConfigHw<IGFX_UNKNOWN>::configureHardwareCustom(HardwareInfo *hwInfo, OSInterface *osIface) {
    PLATFORM *pPlatform = const_cast<PLATFORM *>(hwInfo->pPlatform);
    GT_SYSTEM_INFO *pSysInfo = const_cast<GT_SYSTEM_INFO *>(hwInfo->pSysInfo);
    FeatureTable *pSkuTable = const_cast<FeatureTable *>(hwInfo->pSkuTable);

    if (pPlatform->usDeviceID == 30) {
        pSysInfo->EdramSizeInKb = 128 * 1000;
    }
    if (pPlatform->usDeviceID & hwConfigTestMidThreadBit) {
        pSkuTable->ftrGpGpuMidThreadLevelPreempt = 1;
    }
    if (pPlatform->usDeviceID & hwConfigTestThreadGroupBit) {
        pSkuTable->ftrGpGpuThreadGroupLevelPreempt = 1;
    }
    if (pPlatform->usDeviceID & hwConfigTestMidBatchBit) {
        pSkuTable->ftrGpGpuMidBatchPreempt = 1;
    }
    return (pPlatform->usDeviceID == 10) ? -1 : 0;
}

template <>
void HwInfoConfigHw<IGFX_UNKNOWN>::adjustPlatformForProductFamily(HardwareInfo *hwInfo) {
}

} // namespace OCLRT

struct DummyHwConfig : HwInfoConfigHw<IGFX_UNKNOWN> {
};

using namespace OCLRT;
using namespace std;

void mockCpuidex(int *cpuInfo, int functionId, int subfunctionId);

void HwInfoConfigTestLinux::SetUp() {
    HwInfoConfigTest::SetUp();

    osInterface = new OSInterface();
    drm = new DrmMock();
    osInterface->get()->setDrm(static_cast<Drm *>(drm));

    drm->StoredDeviceID = pOldPlatform->usDeviceID;
    drm->StoredDeviceRevID = 0;
    drm->StoredEUVal = pOldSysInfo->EUCount;
    drm->StoredSSVal = pOldSysInfo->SubSliceCount;

    rt_cpuidex_func = CpuInfo::cpuidexFunc;
    CpuInfo::cpuidexFunc = mockCpuidex;
    testHwInfo = *pInHwInfo;
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

        testPlatform.eRenderCoreFamily = IGFX_UNKNOWN_CORE;
    }

    void TearDown() override {
        HwInfoConfigTestLinux::TearDown();
    }

    DummyHwConfig hwConfig;
};

TEST_F(HwInfoConfigTestLinuxDummy, dummyConfig) {
    int ret = hwConfig.configureHwInfo(pInHwInfo, &outHwInfo, osInterface);
    EXPECT_EQ(0, ret);
}

GTTYPE GtTypes[] = {
    GTTYPE_GT1, GTTYPE_GT2, GTTYPE_GT1_5, GTTYPE_GT2_5, GTTYPE_GT3, GTTYPE_GT4, GTTYPE_GTA, GTTYPE_GTC, GTTYPE_GTX};

TEST_F(HwInfoConfigTestLinuxDummy, dummyConfigGtTypes) {
    int ret = hwConfig.configureHwInfo(pInHwInfo, &outHwInfo, osInterface);
    EXPECT_EQ(0, ret);

    EXPECT_EQ(GTTYPE_GT0, outHwInfo.pPlatform->eGTType);
    EXPECT_EQ(0u, outHwInfo.pSkuTable->ftrGT1);
    EXPECT_EQ(0u, outHwInfo.pSkuTable->ftrGT1_5);
    EXPECT_EQ(0u, outHwInfo.pSkuTable->ftrGT2);
    EXPECT_EQ(0u, outHwInfo.pSkuTable->ftrGT2_5);
    EXPECT_EQ(0u, outHwInfo.pSkuTable->ftrGT3);
    EXPECT_EQ(0u, outHwInfo.pSkuTable->ftrGT4);
    EXPECT_EQ(0u, outHwInfo.pSkuTable->ftrGTA);
    EXPECT_EQ(0u, outHwInfo.pSkuTable->ftrGTC);
    EXPECT_EQ(0u, outHwInfo.pSkuTable->ftrGTX);

    ReleaseOutHwInfoStructs();

    size_t arrSize = sizeof(GtTypes) / sizeof(GTTYPE);
    uint32_t FtrSum = 0;
    for (uint32_t i = 0; i < arrSize; i++) {
        drm->setGtType(GtTypes[i]);
        ret = hwConfig.configureHwInfo(pInHwInfo, &outHwInfo, osInterface);
        EXPECT_EQ(0, ret);
        EXPECT_EQ(GtTypes[i], outHwInfo.pPlatform->eGTType);
        bool FtrPresent = (outHwInfo.pSkuTable->ftrGT1 ||
                           outHwInfo.pSkuTable->ftrGT1_5 ||
                           outHwInfo.pSkuTable->ftrGT2 ||
                           outHwInfo.pSkuTable->ftrGT2_5 ||
                           outHwInfo.pSkuTable->ftrGT3 ||
                           outHwInfo.pSkuTable->ftrGT4 ||
                           outHwInfo.pSkuTable->ftrGTA ||
                           outHwInfo.pSkuTable->ftrGTC ||
                           outHwInfo.pSkuTable->ftrGTX);
        EXPECT_TRUE(FtrPresent);
        FtrSum += (outHwInfo.pSkuTable->ftrGT1 +
                   outHwInfo.pSkuTable->ftrGT1_5 +
                   outHwInfo.pSkuTable->ftrGT2 +
                   outHwInfo.pSkuTable->ftrGT2_5 +
                   outHwInfo.pSkuTable->ftrGT3 +
                   outHwInfo.pSkuTable->ftrGT4 +
                   outHwInfo.pSkuTable->ftrGTA +
                   outHwInfo.pSkuTable->ftrGTC +
                   outHwInfo.pSkuTable->ftrGTX);

        ReleaseOutHwInfoStructs();
    }
    EXPECT_EQ(arrSize, FtrSum);
}

TEST_F(HwInfoConfigTestLinuxDummy, dummyConfigEdramDetection) {
    drm->StoredDeviceID = 30;
    int ret = hwConfig.configureHwInfo(pInHwInfo, &outHwInfo, osInterface);
    EXPECT_EQ(0, ret);
    EXPECT_EQ(1u, outHwInfo.pSkuTable->ftrEDram);
}

TEST_F(HwInfoConfigTestLinuxDummy, givenEnabledPlatformCoherencyWhenConfiguringHwInfoThenIgnoreAndSetAsDisabled) {
    drm->StoredDeviceID = 21;
    int ret = hwConfig.configureHwInfo(pInHwInfo, &outHwInfo, osInterface);
    EXPECT_EQ(0, ret);
    EXPECT_FALSE(outHwInfo.capabilityTable.ftrSupportsCoherency);
}

TEST_F(HwInfoConfigTestLinuxDummy, givenDisabledPlatformCoherencyWhenConfiguringHwInfoThenSetValidCapability) {
    drm->StoredDeviceID = 20;
    int ret = hwConfig.configureHwInfo(pInHwInfo, &outHwInfo, osInterface);
    EXPECT_EQ(0, ret);
    EXPECT_FALSE(outHwInfo.capabilityTable.ftrSupportsCoherency);
}

TEST_F(HwInfoConfigTestLinuxDummy, dummyNegativeUnknownGtType) {
    drm->setGtType(GTTYPE_UNDEFINED);

    int ret = hwConfig.configureHwInfo(pInHwInfo, &outHwInfo, osInterface);
    EXPECT_EQ(-1, ret);
}

TEST_F(HwInfoConfigTestLinuxDummy, dummyNegativeUnknownDevId) {
    drm->StoredDeviceID = 0;

    int ret = hwConfig.configureHwInfo(pInHwInfo, &outHwInfo, osInterface);
    EXPECT_EQ(-1, ret);
}

TEST_F(HwInfoConfigTestLinuxDummy, dummyNegativeFailGetDevId) {
    drm->StoredRetValForDeviceID = -2;

    int ret = hwConfig.configureHwInfo(pInHwInfo, &outHwInfo, osInterface);
    EXPECT_EQ(-2, ret);
}

TEST_F(HwInfoConfigTestLinuxDummy, dummydummyNegativeFailGetDevRevId) {
    drm->StoredRetValForDeviceRevID = -3;

    int ret = hwConfig.configureHwInfo(pInHwInfo, &outHwInfo, osInterface);
    EXPECT_EQ(-3, ret);
}

TEST_F(HwInfoConfigTestLinuxDummy, dummydummyNegativeFailGetEuCount) {
    drm->StoredRetValForEUVal = -4;

    int ret = hwConfig.configureHwInfo(pInHwInfo, &outHwInfo, osInterface);
    EXPECT_EQ(-4, ret);
}

TEST_F(HwInfoConfigTestLinuxDummy, dummydummyNegativeFailGetSsCount) {
    drm->StoredRetValForSSVal = -5;

    int ret = hwConfig.configureHwInfo(pInHwInfo, &outHwInfo, osInterface);
    EXPECT_EQ(-5, ret);
}

TEST_F(HwInfoConfigTestLinuxDummy, dummyNegativeFailingConfigureCustom) {
    drm->StoredDeviceID = 10;

    int ret = hwConfig.configureHwInfo(pInHwInfo, &outHwInfo, osInterface);
    EXPECT_EQ(-1, ret);
}

TEST_F(HwInfoConfigTestLinuxDummy, dummyNegativeUnknownDeviceId) {
    drm->StoredDeviceID = 0;
    drm->setGtType(GTTYPE_GT1);

    auto hwConfig = DummyHwConfig{};
    int ret = hwConfig.configureHwInfo(pInHwInfo, &outHwInfo, osInterface);
    EXPECT_EQ(-1, ret);
}

TEST_F(HwInfoConfigTestLinuxDummy, dummyConfigPreemptionDrmEnabledMidThreadOn) {
    pInHwInfo->capabilityTable.defaultPreemptionMode = PreemptionMode::MidThread;
    drm->StoredPreemptionSupport = 1;
    drm->StoredMockPreemptionSupport = 1;
    drm->StoredDeviceID = hwConfigTestMidThreadBit;
    int ret = hwConfig.configureHwInfo(pInHwInfo, &outHwInfo, osInterface);
    EXPECT_EQ(0, ret);
    EXPECT_EQ(PreemptionMode::MidThread, outHwInfo.capabilityTable.defaultPreemptionMode);
}

TEST_F(HwInfoConfigTestLinuxDummy, dummyConfigPreemptionDrmEnabledThreadGroupOn) {
    pInHwInfo->capabilityTable.defaultPreemptionMode = PreemptionMode::MidThread;
    drm->StoredPreemptionSupport = 1;
    drm->StoredMockPreemptionSupport = 1;
    drm->StoredDeviceID = hwConfigTestThreadGroupBit;
    int ret = hwConfig.configureHwInfo(pInHwInfo, &outHwInfo, osInterface);
    EXPECT_EQ(0, ret);
    EXPECT_EQ(PreemptionMode::ThreadGroup, outHwInfo.capabilityTable.defaultPreemptionMode);
}

TEST_F(HwInfoConfigTestLinuxDummy, dummyConfigPreemptionDrmEnabledMidBatchOn) {
    pInHwInfo->capabilityTable.defaultPreemptionMode = PreemptionMode::MidThread;
    drm->StoredPreemptionSupport = 1;
    drm->StoredMockPreemptionSupport = 1;
    drm->StoredDeviceID = hwConfigTestMidBatchBit;
    int ret = hwConfig.configureHwInfo(pInHwInfo, &outHwInfo, osInterface);
    EXPECT_EQ(0, ret);
    EXPECT_EQ(PreemptionMode::MidBatch, outHwInfo.capabilityTable.defaultPreemptionMode);
}

TEST_F(HwInfoConfigTestLinuxDummy, dummyConfigPreemptionDrmEnabledNoPreemption) {
    pInHwInfo->capabilityTable.defaultPreemptionMode = PreemptionMode::MidThread;
    drm->StoredPreemptionSupport = 1;
    drm->StoredMockPreemptionSupport = 1;
    drm->StoredDeviceID = 1;
    int ret = hwConfig.configureHwInfo(pInHwInfo, &outHwInfo, osInterface);
    EXPECT_EQ(0, ret);
    EXPECT_EQ(PreemptionMode::Disabled, outHwInfo.capabilityTable.defaultPreemptionMode);
}

TEST_F(HwInfoConfigTestLinuxDummy, dummyConfigPreemptionDrmDisabledAllPreemption) {
    pInHwInfo->capabilityTable.defaultPreemptionMode = PreemptionMode::MidThread;
    drm->StoredPreemptionSupport = 0;
    drm->StoredMockPreemptionSupport = 0;
    drm->StoredDeviceID = hwConfigTestMidThreadBit | hwConfigTestThreadGroupBit | hwConfigTestMidBatchBit;
    int ret = hwConfig.configureHwInfo(pInHwInfo, &outHwInfo, osInterface);
    EXPECT_EQ(0, ret);
    EXPECT_EQ(PreemptionMode::Disabled, outHwInfo.capabilityTable.defaultPreemptionMode);
}

TEST_F(HwInfoConfigTestLinuxDummy, dummyConfigPreemptionDrmEnabledAllPreemptionDriverThreadGroup) {
    pInHwInfo->capabilityTable.defaultPreemptionMode = PreemptionMode::ThreadGroup;
    drm->StoredPreemptionSupport = 1;
    drm->StoredMockPreemptionSupport = 1;
    drm->StoredDeviceID = hwConfigTestMidThreadBit | hwConfigTestThreadGroupBit | hwConfigTestMidBatchBit;
    int ret = hwConfig.configureHwInfo(pInHwInfo, &outHwInfo, osInterface);
    EXPECT_EQ(0, ret);
    EXPECT_EQ(PreemptionMode::ThreadGroup, outHwInfo.capabilityTable.defaultPreemptionMode);
}

TEST_F(HwInfoConfigTestLinuxDummy, dummyConfigPreemptionDrmEnabledAllPreemptionDriverMidBatch) {
    pInHwInfo->capabilityTable.defaultPreemptionMode = PreemptionMode::MidBatch;
    drm->StoredPreemptionSupport = 1;
    drm->StoredMockPreemptionSupport = 1;
    drm->StoredDeviceID = hwConfigTestMidThreadBit | hwConfigTestThreadGroupBit | hwConfigTestMidBatchBit;
    int ret = hwConfig.configureHwInfo(pInHwInfo, &outHwInfo, osInterface);
    EXPECT_EQ(0, ret);
    EXPECT_EQ(PreemptionMode::MidBatch, outHwInfo.capabilityTable.defaultPreemptionMode);
}

TEST_F(HwInfoConfigTestLinuxDummy, dummyConfigPreemptionDrmEnabledAllPreemptionDriverDisabled) {
    pInHwInfo->capabilityTable.defaultPreemptionMode = PreemptionMode::Disabled;
    drm->StoredPreemptionSupport = 1;
    drm->StoredMockPreemptionSupport = 1;
    drm->StoredDeviceID = hwConfigTestMidThreadBit | hwConfigTestThreadGroupBit | hwConfigTestMidBatchBit;
    int ret = hwConfig.configureHwInfo(pInHwInfo, &outHwInfo, osInterface);
    EXPECT_EQ(0, ret);
    EXPECT_EQ(PreemptionMode::Disabled, outHwInfo.capabilityTable.defaultPreemptionMode);
}

TEST_F(HwInfoConfigTestLinuxDummy, givenPlatformEnabledFtrCompressionWhenInitializingThenForceDisable) {
    pInHwInfo->capabilityTable.ftrRenderCompressedBuffers = true;
    pInHwInfo->capabilityTable.ftrRenderCompressedImages = true;
    int ret = hwConfig.configureHwInfo(pInHwInfo, &outHwInfo, osInterface);
    EXPECT_EQ(0, ret);
    EXPECT_FALSE(outHwInfo.capabilityTable.ftrRenderCompressedBuffers);
    EXPECT_FALSE(outHwInfo.capabilityTable.ftrRenderCompressedImages);
}

TEST_F(HwInfoConfigTestLinuxDummy, givenPointerToHwInfoWhenConfigureHwInfoCalledThenRequiedSurfaceSizeIsSettedProperly) {
    EXPECT_EQ(MemoryConstants::pageSize, pInHwInfo->capabilityTable.requiredPreemptionSurfaceSize);
    int ret = hwConfig.configureHwInfo(pInHwInfo, &outHwInfo, osInterface);
    EXPECT_EQ(0, ret);
    EXPECT_EQ(outHwInfo.pSysInfo->CsrSizeInMb * MemoryConstants::megaByte, outHwInfo.capabilityTable.requiredPreemptionSurfaceSize);
}
