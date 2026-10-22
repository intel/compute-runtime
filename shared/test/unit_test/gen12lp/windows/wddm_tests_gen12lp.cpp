/*
 * Copyright (C) 2020-2026 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/command_stream/preemption.h"
#include "shared/source/execution_environment/root_device_environment.h"
#include "shared/source/os_interface/os_interface.h"
#include "shared/source/os_interface/windows/gdi_interface.h"
#include "shared/test/common/mocks/mock_execution_environment.h"
#include "shared/test/common/mocks/mock_wddm.h"
#include "shared/test/common/os_interface/windows/gdi_dll_fixture.h"
#include "shared/test/common/os_interface/windows/wddm_fixture.h"
#include "shared/test/common/test_macros/hw_test.h"

using namespace NEO;

using Gen12LpWddmHwInfoTest = ::testing::Test;

GEN12LPTEST_F(Gen12LpWddmHwInfoTest, givenIncorrectProductFamiliyWhenInitCalledThenOverride) {
    HardwareInfo localHwInfo = *defaultHwInfo;

    localHwInfo.platform.eRenderCoreFamily = GFXCORE_FAMILY::IGFX_UNKNOWN_CORE;

    setAdapterInfo(&localHwInfo);

    auto executionEnvironment = std::make_unique<MockExecutionEnvironment>();
    auto rootDeviceEnvironment = executionEnvironment->rootDeviceEnvironments[0].get();
    rootDeviceEnvironment->osInterface = std::make_unique<OSInterface>();

    auto localWddm = std::unique_ptr<Wddm>(Wddm::createWddm(nullptr, *rootDeviceEnvironment));
    localWddm->init();

    auto newHwInfo = rootDeviceEnvironment->getHardwareInfo();

    EXPECT_EQ(GFXCORE_FAMILY::IGFX_GEN12LP_CORE, newHwInfo->platform.eRenderCoreFamily);

    // reset mock gdi globals
    localHwInfo = *defaultHwInfo;
    setAdapterInfo(&localHwInfo);
}

using Gen12LpWddmTranslateFromKmdTest = WddmTestWithMockGdiDll;

GEN12LPTEST_F(Gen12LpWddmTranslateFromKmdTest, givenGen12lpProductWhenTranslatingFromKmdThenRenderCoreFamilyIsSetToGen12lp) {
    auto platform = defaultHwInfo->platform;
    for (auto productFamily : {IGFX_TIGERLAKE_LP, IGFX_ROCKETLAKE, IGFX_ALDERLAKE_S, IGFX_ALDERLAKE_P, IGFX_ALDERLAKE_N, IGFX_DG1}) {
        platform.eProductFamily = productFamily;
        platform.eRenderCoreFamily = IGFX_GEN11_CORE;
        wddm->translateFromKmd(platform);
        EXPECT_EQ(productFamily, platform.eProductFamily);
        EXPECT_EQ(IGFX_GEN12LP_CORE, platform.eRenderCoreFamily);
    }
}
