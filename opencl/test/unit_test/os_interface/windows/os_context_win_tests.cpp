/*
 * Copyright (C) 2019-2020 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/test/unit_test/helpers/debug_manager_state_restore.h"

#include "opencl/test/unit_test/mocks/mock_wddm.h"
#include "opencl/test/unit_test/os_interface/windows/wddm_fixture.h"

using namespace NEO;

struct OsContextWinTest : public WddmTestWithMockGdiDll {
    void SetUp() override {
        WddmTestWithMockGdiDll::SetUp();
        preemptionMode = PreemptionHelper::getDefaultPreemptionMode(*platformDevices[0]);
        engineType = HwHelper::get(platformDevices[0]->platform.eRenderCoreFamily).getGpgpuEngineInstances(*platformDevices[0])[0];

        init();
    }

    PreemptionMode preemptionMode;
    aub_stream::EngineType engineType;
};

TEST_F(OsContextWinTest, givenWddm20WhenCreatingOsContextThenOsContextIsInitialized) {
    osContext = std::make_unique<OsContextWin>(*osInterface->get()->getWddm(), 0u, 1, engineType, preemptionMode, false);
    EXPECT_TRUE(osContext->isInitialized());
}

TEST_F(OsContextWinTest, givenWddm20WhenCreatingWddmContextFailThenOsContextIsNotInitialized) {
    wddm->device = INVALID_HANDLE;

    osContext = std::make_unique<OsContextWin>(*osInterface->get()->getWddm(), 0u, 1, engineType, preemptionMode, false);
    EXPECT_FALSE(osContext->isInitialized());
}

TEST_F(OsContextWinTest, givenWddm20WhenCreatingWddmMonitorFenceFailThenOsContextIsNotInitialized) {
    *getCreateSynchronizationObject2FailCallFcn() = true;

    osContext = std::make_unique<OsContextWin>(*osInterface->get()->getWddm(), 0u, 1, engineType, preemptionMode, false);
    EXPECT_FALSE(osContext->isInitialized());
}

TEST_F(OsContextWinTest, givenWddm20WhenRegisterTrimCallbackFailThenOsContextIsNotInitialized) {
    *getRegisterTrimNotificationFailCallFcn() = true;

    osContext = std::make_unique<OsContextWin>(*osInterface->get()->getWddm(), 0u, 1, engineType, preemptionMode, false);
    EXPECT_FALSE(osContext->isInitialized());
}

TEST_F(OsContextWinTest, givenWddm20WhenRegisterTrimCallbackIsDisabledThenOsContextIsInitialized) {
    DebugManagerStateRestore stateRestore;
    DebugManager.flags.DoNotRegisterTrimCallback.set(true);
    *getRegisterTrimNotificationFailCallFcn() = true;

    osContext = std::make_unique<OsContextWin>(*osInterface->get()->getWddm(), 0u, 1, engineType, preemptionMode, false);
    EXPECT_TRUE(osContext->isInitialized());
}
