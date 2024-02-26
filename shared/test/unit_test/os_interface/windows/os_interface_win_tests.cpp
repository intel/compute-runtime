/*
 * Copyright (C) 2018-2024 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/test/unit_test/os_interface/windows/os_interface_win_tests.h"

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
    auto &gfxCoreHelper = rootDeviceEnvironment.getHelper<GfxCoreHelper>();
    auto osContext = std::make_unique<OsContextWin>(*wddm, 0, 0u,
                                                    EngineDescriptorHelper::getDefaultDescriptor(gfxCoreHelper.getGpgpuEngineInstances(rootDeviceEnvironment)[0], preemptionMode));
    osContext->ensureContextInitialized();
    EXPECT_EQ(osContext->getWddm(), wddm);
    EXPECT_EQ(1u, wddm->registerTrimCallbackResult.called);
}

TEST_F(OsInterfaceTest, GivenWindowsOsWhenCheckForNewResourceImplicitFlushSupportThenReturnTrue) {
    EXPECT_TRUE(OSInterface::newResourceImplicitFlush);
}

TEST_F(OsInterfaceTest, GivenWindowsOsWhenCheckForGpuIdleImplicitFlushSupportThenReturnFalse) {
    EXPECT_FALSE(OSInterface::gpuIdleImplicitFlush);
}

TEST_F(OsInterfaceTest, GivenDefaultOsInterfaceThenLocalMemoryEnabled) {
    EXPECT_TRUE(OSInterface::osEnableLocalMemory);
}

TEST_F(OsInterfaceTest, whenOsInterfaceSetupGmmInputArgsThenArgsAreSet) {
    MockExecutionEnvironment executionEnvironment;
    RootDeviceEnvironment rootDeviceEnvironment(executionEnvironment);
    auto wddm = new WddmMock(rootDeviceEnvironment);
    EXPECT_EQ(nullptr, rootDeviceEnvironment.osInterface.get());
    wddm->init();
    EXPECT_NE(nullptr, rootDeviceEnvironment.osInterface.get());

    constexpr auto expectedCoreFamily = IGFX_GEN12_CORE;

    wddm->gfxPlatform->eRenderCoreFamily = expectedCoreFamily;
    wddm->gfxPlatform->eDisplayCoreFamily = expectedCoreFamily;
    wddm->deviceRegistryPath = "registryPath";
    auto expectedRegistryPath = wddm->deviceRegistryPath.c_str();
    auto &adapterBDF = wddm->adapterBDF;
    uint32_t bus = 0x12;
    adapterBDF.Bus = bus;
    uint32_t device = 0x34;
    adapterBDF.Device = device;
    uint32_t function = 0x56;
    adapterBDF.Function = function;

    GMM_INIT_IN_ARGS gmmInputArgs = {};
    EXPECT_NE(0, memcmp(&wddm->adapterBDF, &gmmInputArgs.stAdapterBDF, sizeof(ADAPTER_BDF)));
    EXPECT_STRNE(expectedRegistryPath, gmmInputArgs.DeviceRegistryPath);

    rootDeviceEnvironment.osInterface->getDriverModel()->setGmmInputArgs(&gmmInputArgs);

    EXPECT_EQ(0, memcmp(&wddm->adapterBDF, &gmmInputArgs.stAdapterBDF, sizeof(ADAPTER_BDF)));
    EXPECT_EQ(GMM_CLIENT::GMM_OCL_VISTA, gmmInputArgs.ClientType);
    EXPECT_STREQ(expectedRegistryPath, gmmInputArgs.DeviceRegistryPath);
    EXPECT_EQ(expectedCoreFamily, gmmInputArgs.Platform.eRenderCoreFamily);
    EXPECT_EQ(expectedCoreFamily, gmmInputArgs.Platform.eDisplayCoreFamily);
}