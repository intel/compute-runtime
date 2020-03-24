/*
 * Copyright (C) 2017-2020 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "opencl/test/unit_test/os_interface/windows/os_interface_win_tests.h"

#include "shared/source/execution_environment/root_device_environment.h"
#include "shared/source/os_interface/windows/os_context_win.h"

#include "opencl/test/unit_test/mocks/mock_execution_environment.h"
#include "opencl/test/unit_test/os_interface/windows/wddm_fixture.h"

TEST_F(OsInterfaceTest, GivenWindowsWhenOsSupportFor64KBpagesIsBeingQueriedThenTrueIsReturned) {
    EXPECT_TRUE(OSInterface::are64kbPagesEnabled());
}

TEST_F(OsInterfaceTest, GivenWindowsWhenCreateEentIsCalledThenValidEventHandleIsReturned) {
    auto ev = osInterface->get()->createEvent(NULL, TRUE, FALSE, "DUMMY_EVENT_NAME");
    EXPECT_NE(nullptr, ev);
    auto ret = osInterface->get()->closeHandle(ev);
    EXPECT_EQ(TRUE, ret);
}

TEST(OsContextTest, givenWddmWhenCreateOsContextAfterInitWddmThenOsContextIsInitializedTrimCallbackIsRegisteredMemoryOperationsHandlerCreated) {
    MockExecutionEnvironment executionEnvironment;
    RootDeviceEnvironment rootDeviceEnvironment(executionEnvironment);
    auto wddm = new WddmMock(rootDeviceEnvironment);
    OSInterface osInterface;
    osInterface.get()->setWddm(wddm);
    auto preemptionMode = PreemptionHelper::getDefaultPreemptionMode(*defaultHwInfo);
    wddm->init();
    EXPECT_EQ(0u, wddm->registerTrimCallbackResult.called);
    auto osContext = std::make_unique<OsContextWin>(*wddm, 0u, 1,
                                                    HwHelper::get(defaultHwInfo->platform.eRenderCoreFamily).getGpgpuEngineInstances(*defaultHwInfo)[0],
                                                    preemptionMode, false, false, false);
    EXPECT_TRUE(osContext->isInitialized());
    EXPECT_EQ(osContext->getWddm(), wddm);
    EXPECT_EQ(1u, wddm->registerTrimCallbackResult.called);
}
