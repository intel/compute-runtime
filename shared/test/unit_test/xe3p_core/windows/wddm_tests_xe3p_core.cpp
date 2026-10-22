/*
 * Copyright (C) 2026 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/test/common/helpers/debug_manager_state_restore.h"
#include "shared/test/common/os_interface/windows/wddm_fixture.h"
#include "shared/test/common/test_macros/hw_test.h"

using namespace NEO;

using WddmTestsXe3pCore = WddmTestWithMockGdiDll;

XE3P_CORETEST_F(WddmTestsXe3pCore, givenXe3pProductWhenTranslatingFromKmdThenRenderCoreFamilyIsSetToXe3p) {
    auto platform = defaultHwInfo->platform;
    for (auto productFamily : {IGFX_NVL, IGFX_CRI}) {
        platform.eProductFamily = productFamily;
        platform.eRenderCoreFamily = IGFX_UNKNOWN_CORE;
        wddm->translateFromKmd(platform);
        EXPECT_EQ(productFamily, platform.eProductFamily);
        EXPECT_EQ(IGFX_XE3P_CORE, platform.eRenderCoreFamily);
    }
}

XE3P_CORETEST_F(WddmTestsXe3pCore, givenXe3pProductAndForceCoreFamilyWhenTranslatingFromKmdThenForcedCoreFamilyIsSet) {
    DebugManagerStateRestore restorer{};
    debugManager.flags.ForceCoreFamily.set(std::to_string(IGFX_GEN12LP_CORE));

    auto platform = defaultHwInfo->platform;
    platform.eProductFamily = IGFX_NVL;
    platform.eRenderCoreFamily = IGFX_UNKNOWN_CORE;
    wddm->translateFromKmd(platform);
    EXPECT_EQ(IGFX_NVL, platform.eProductFamily);
    EXPECT_EQ(IGFX_GEN12LP_CORE, platform.eRenderCoreFamily);
}

XE3P_CORETEST_F(WddmTestsXe3pCore, givenUnknownCoreFamilyInAdapterInfoWhenInitializingWddmThenHwInfoHasXe3pCoreAndGfxPlatformIsNotChanged) {
    HardwareInfo localHwInfo = *defaultHwInfo;
    localHwInfo.platform.eRenderCoreFamily = IGFX_UNKNOWN_CORE;
    setAdapterInfo(&localHwInfo);

    init();

    EXPECT_EQ(IGFX_XE3P_CORE, rootDeviceEnvironment->getHardwareInfo()->platform.eRenderCoreFamily);
    EXPECT_EQ(IGFX_UNKNOWN_CORE, wddm->gfxPlatform->eRenderCoreFamily);

    setAdapterInfo(defaultHwInfo.get());
}
