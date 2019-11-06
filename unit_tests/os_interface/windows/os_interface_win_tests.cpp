/*
 * Copyright (C) 2017-2019 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "unit_tests/os_interface/windows/os_interface_win_tests.h"

#include "core/execution_environment/root_device_environment.h"
#include "runtime/os_interface/windows/os_context_win.h"
#include "unit_tests/mocks/mock_execution_environment.h"
#include "unit_tests/os_interface/windows/wddm_fixture.h"

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
    auto preemptionMode = PreemptionHelper::getDefaultPreemptionMode(*platformDevices[0]);
    auto hwInfo = *platformDevices[0];
    wddm->init(hwInfo);
    EXPECT_EQ(0u, wddm->registerTrimCallbackResult.called);
    auto osContext = std::make_unique<OsContextWin>(*wddm, 0u, 1, HwHelper::get(platformDevices[0]->platform.eRenderCoreFamily).getGpgpuEngineInstances()[0], preemptionMode, false);
    EXPECT_TRUE(osContext->isInitialized());
    EXPECT_EQ(osContext->getWddm(), wddm);
    EXPECT_EQ(1u, wddm->registerTrimCallbackResult.called);
}

TEST_F(OsInterfaceTest, whenOsInterfaceSetupGmmInputArgsThenProperAdapterBDFIsSet) {
    MockExecutionEnvironment executionEnvironment;
    RootDeviceEnvironment rootDeviceEnvironment(executionEnvironment);
    auto wddm = new WddmMock(rootDeviceEnvironment);
    osInterface->get()->setWddm(wddm);
    auto hwInfo = *platformDevices[0];
    wddm->init(hwInfo);
    auto &adapterBDF = wddm->adapterBDF;
    adapterBDF.Bus = 0x12;
    adapterBDF.Device = 0x34;
    adapterBDF.Function = 0x56;
    GMM_INIT_IN_ARGS gmmInputArgs = {};
    EXPECT_NE(0, memcmp(&adapterBDF, &gmmInputArgs.stAdapterBDF, sizeof(ADAPTER_BDF)));
    osInterface->setGmmInputArgs(&gmmInputArgs);
    EXPECT_EQ(0, memcmp(&adapterBDF, &gmmInputArgs.stAdapterBDF, sizeof(ADAPTER_BDF)));
}
