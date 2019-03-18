/*
 * Copyright (C) 2017-2019 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "unit_tests/os_interface/windows/os_interface_win_tests.h"

#include "runtime/os_interface/windows/os_context_win.h"
#include "unit_tests/os_interface/windows/wddm_fixture.h"

TEST_F(OsInterfaceTest, givenOsInterfaceWithoutWddmWhenGetHwContextIdIsCalledThenReturnsZero) {
    auto retVal = osInterface->getHwContextId();
    EXPECT_EQ(0u, retVal);
}

TEST_F(OsInterfaceTest, GivenWindowsWhenOsSupportFor64KBpagesIsBeingQueriedThenTrueIsReturned) {
    EXPECT_TRUE(OSInterface::are64kbPagesEnabled());
}

TEST_F(OsInterfaceTest, GivenWindowsWhenCreateEentIsCalledThenValidEventHandleIsReturned) {
    auto ev = osInterface->get()->createEvent(NULL, TRUE, FALSE, "DUMMY_EVENT_NAME");
    EXPECT_NE(nullptr, ev);
    auto ret = osInterface->get()->closeHandle(ev);
    EXPECT_EQ(TRUE, ret);
}

TEST(OsContextTest, givenWddmWhenCreateOsContextBeforeInitWddmThenOsContextIsNotInitialized) {
    auto wddm = new WddmMock;
    OSInterface osInterface;
    osInterface.get()->setWddm(wddm);
    EXPECT_THROW(OsContextWin(*wddm, 0u, 1, HwHelper::get(platformDevices[0]->pPlatform->eRenderCoreFamily).getGpgpuEngineInstances()[0],
                              PreemptionHelper::getDefaultPreemptionMode(*platformDevices[0]), false),
                 std::exception);
}

TEST(OsContextTest, givenWddmWhenCreateOsContextAfterInitWddmThenOsContextIsInitializedAndTrimCallbackIsRegistered) {
    auto wddm = new WddmMock;
    OSInterface osInterface;
    osInterface.get()->setWddm(wddm);
    auto preemptionMode = PreemptionHelper::getDefaultPreemptionMode(*platformDevices[0]);
    wddm->init(preemptionMode);
    EXPECT_EQ(0u, wddm->registerTrimCallbackResult.called);
    auto osContext = std::make_unique<OsContextWin>(*wddm, 0u, 1, HwHelper::get(platformDevices[0]->pPlatform->eRenderCoreFamily).getGpgpuEngineInstances()[0], preemptionMode, false);
    EXPECT_TRUE(osContext->isInitialized());
    EXPECT_EQ(osContext->getWddm(), wddm);
    EXPECT_EQ(1u, wddm->registerTrimCallbackResult.called);
}
