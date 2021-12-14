/*
 * Copyright (C) 2020-2021 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/test/common/test_macros/test_checks_shared.h"

#include "shared/source/device/device.h"
#include "shared/source/helpers/hw_helper.h"
#include "shared/source/os_interface/hw_info_config.h"
#include "shared/test/common/helpers/default_hw_info.h"
#include "shared/test/common/test_macros/test.h"

using namespace NEO;

bool TestChecks::supportsBlitter(const HardwareInfo *pHardwareInfo) {
    auto engines = HwHelper::get(::renderCoreFamily).getGpgpuEngineInstances(*pHardwareInfo);
    for (const auto &engine : engines) {
        if (engine.first == aub_stream::EngineType::ENGINE_BCS) {
            return pHardwareInfo->capabilityTable.blitterOperationsSupported;
        }
    }
    return false;
}

bool TestChecks::fullySupportsBlitter(const HardwareInfo *pHardwareInfo) {
    auto engines = HwHelper::get(::renderCoreFamily).getGpgpuEngineInstances(*pHardwareInfo);
    for (const auto &engine : engines) {
        if (engine.first == aub_stream::EngineType::ENGINE_BCS) {
            return HwInfoConfig::get(pHardwareInfo->platform.eProductFamily)->isBlitterFullySupported(*pHardwareInfo);
        }
    }
    return false;
}

bool TestChecks::supportsImages(const HardwareInfo &hardwareInfo) {
    return hardwareInfo.capabilityTable.supportsImages;
}

bool TestChecks::supportsImages(const std::unique_ptr<HardwareInfo> &pHardwareInfo) {
    return supportsImages(*pHardwareInfo);
}

bool TestChecks::supportsSvm(const HardwareInfo *pHardwareInfo) {
    return pHardwareInfo->capabilityTable.ftrSvm;
}
bool TestChecks::supportsSvm(const std::unique_ptr<HardwareInfo> &pHardwareInfo) {
    return supportsSvm(pHardwareInfo.get());
}
bool TestChecks::supportsSvm(const Device *pDevice) {
    return supportsSvm(&pDevice->getHardwareInfo());
}

class TestMacrosIfNotMatchTearDownCall : public ::testing::Test {
  public:
    void expectCorrectPlatform() {
        EXPECT_EQ(IGFX_SKYLAKE, defaultHwInfo->platform.eProductFamily);
    }
    void SetUp() override {
        expectCorrectPlatform();
    }
    void TearDown() override {
        expectCorrectPlatform();
    }
};

HWTEST2_F(TestMacrosIfNotMatchTearDownCall, givenNotMatchPlatformWhenUseHwTest2FThenSetUpAndTearDownAreNotCalled, IsSKL) {
    expectCorrectPlatform();
}
class TestMacrosWithParamIfNotMatchTearDownCall : public TestMacrosIfNotMatchTearDownCall, public ::testing::WithParamInterface<int> {};
HWTEST2_P(TestMacrosWithParamIfNotMatchTearDownCall, givenNotMatchPlatformWhenUseHwTest2PThenSetUpAndTearDownAreNotCalled, IsSKL) {
    expectCorrectPlatform();
}
INSTANTIATE_TEST_CASE_P(givenNotMatchPlatformWhenUseHwTest2PThenSetUpAndTearDownAreNotCalled,
                        TestMacrosWithParamIfNotMatchTearDownCall,
                        ::testing::Values(0));
