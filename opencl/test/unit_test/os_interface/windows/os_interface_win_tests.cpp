/*
 * Copyright (C) 2018-2021 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "opencl/test/unit_test/os_interface/windows/os_interface_win_tests.h"

#include "shared/source/execution_environment/root_device_environment.h"
#include "shared/source/os_interface/windows/os_context_win.h"
#include "shared/source/os_interface/windows/sys_calls.h"
#include "shared/test/common/helpers/engine_descriptor_helper.h"
#include "shared/test/common/mocks/mock_execution_environment.h"
#include "shared/test/common/os_interface/windows/wddm_fixture.h"

TEST_F(OsInterfaceTest, GivenWindowsWhenOsSupportFor64KBpagesIsBeingQueriedThenTrueIsReturned) {
    EXPECT_TRUE(OSInterface::are64kbPagesEnabled());
}

TEST_F(OsInterfaceTest, GivenWindowsWhenCreateEentIsCalledThenValidEventHandleIsReturned) {
    auto ev = NEO::SysCalls::createEvent(NULL, TRUE, FALSE, "DUMMY_EVENT_NAME");
    EXPECT_NE(nullptr, ev);
    auto ret = NEO::SysCalls::closeHandle(ev);
    EXPECT_EQ(TRUE, ret);
}

TEST(OsContextTest, givenWddmWhenCreateOsContextAfterInitWddmThenOsContextIsInitializedTrimCallbackIsRegisteredMemoryOperationsHandlerCreated) {
    MockExecutionEnvironment executionEnvironment;
    RootDeviceEnvironment rootDeviceEnvironment(executionEnvironment);
    auto wddm = new WddmMock(rootDeviceEnvironment);
    auto preemptionMode = PreemptionHelper::getDefaultPreemptionMode(*defaultHwInfo);
    wddm->init();
    EXPECT_EQ(0u, wddm->registerTrimCallbackResult.called);
    auto osContext = std::make_unique<OsContextWin>(*wddm, 0u,
                                                    EngineDescriptorHelper::getDefaultDescriptor(HwHelper::get(defaultHwInfo->platform.eRenderCoreFamily).getGpgpuEngineInstances(*defaultHwInfo)[0], preemptionMode));
    osContext->ensureContextInitialized();
    EXPECT_EQ(osContext->getWddm(), wddm);
    EXPECT_EQ(1u, wddm->registerTrimCallbackResult.called);
}

TEST_F(OsInterfaceTest, GivenWindowsOsWhenCheckForNewResourceImplicitFlushSupportThenReturnFalse) {
    EXPECT_FALSE(OSInterface::newResourceImplicitFlush);
}

TEST_F(OsInterfaceTest, GivenWindowsOsWhenCheckForGpuIdleImplicitFlushSupportThenReturnFalse) {
    EXPECT_FALSE(OSInterface::gpuIdleImplicitFlush);
}
