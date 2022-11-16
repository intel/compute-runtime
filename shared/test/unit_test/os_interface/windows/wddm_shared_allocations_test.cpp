/*
 * Copyright (C) 2021-2022 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/gmm_helper/gmm.h"
#include "shared/test/common/fixtures/mock_execution_environment_gmm_fixture.h"
#include "shared/test/common/mocks/mock_wddm.h"
#include "shared/test/common/os_interface/windows/wddm_fixture.h"
#include "shared/test/common/test_macros/hw_test.h"

using namespace NEO;

class WddmSharedAllocationsMock : public WddmMock {
  public:
    WddmSharedAllocationsMock(RootDeviceEnvironment &rootDeviceEnvironment) : WddmMock(rootDeviceEnvironment) {}
    WddmSharedAllocationsMock(std::unique_ptr<HwDeviceIdWddm> &&hwDeviceId, RootDeviceEnvironment &rootDeviceEnvironment) : WddmMock(std::move(hwDeviceId), rootDeviceEnvironment) {}
    NTSTATUS createNTHandle(const D3DKMT_HANDLE *resourceHandle, HANDLE *ntHandle) override {
        return static_cast<NTSTATUS>(0xfff);
    }
};

class WddmSharedTestsFixture : public GdiDllFixture, public MockExecutionEnvironmentGmmFixture {
  public:
    void setUp() {
        MockExecutionEnvironmentGmmFixture::setUp();
        GdiDllFixture::setUp();
        rootDeviceEnvironment = executionEnvironment->rootDeviceEnvironments[0].get();
        wddm = new WddmSharedAllocationsMock(*rootDeviceEnvironment);
        wddmMockInterface = new WddmMockInterface20(*wddm);
        wddm->wddmInterface.reset(wddmMockInterface);
        rootDeviceEnvironment->osInterface = std::make_unique<OSInterface>();
        rootDeviceEnvironment->osInterface->setDriverModel(std::unique_ptr<DriverModel>(wddm));
        rootDeviceEnvironment->memoryOperationsInterface = std::make_unique<WddmMemoryOperationsHandler>(wddm);
        osInterface = rootDeviceEnvironment->osInterface.get();
    }

    void init() {
        auto preemptionMode = PreemptionHelper::getDefaultPreemptionMode(*defaultHwInfo);
        wddmMockInterface = static_cast<WddmMockInterface20 *>(wddm->wddmInterface.release());
        wddm->init();
        wddm->wddmInterface.reset(wddmMockInterface);

        auto hwInfo = rootDeviceEnvironment->getHardwareInfo();
        auto engine = HwHelper::get(defaultHwInfo->platform.eRenderCoreFamily).getGpgpuEngineInstances(*hwInfo)[0];
        osContext = std::make_unique<OsContextWin>(*osInterface->getDriverModel()->as<Wddm>(), 0, 0u, EngineDescriptorHelper::getDefaultDescriptor(engine, preemptionMode));
        osContext->ensureContextInitialized();
    }

    void tearDown() {
        GdiDllFixture::tearDown();
    }

    WddmSharedAllocationsMock *wddm = nullptr;
    OSInterface *osInterface;
    std::unique_ptr<OsContextWin> osContext;
    WddmMockInterface20 *wddmMockInterface = nullptr;
    RootDeviceEnvironment *rootDeviceEnvironment = nullptr;
};

using WdmmSharedTests = Test<WddmSharedTestsFixture>;

TEST_F(WdmmSharedTests, WhenCreatingSharedAllocationAndGetNTHandleFailedThenAllocationIsDeletedAndHandlesAreSetToZero) {
    init();

    D3DKMT_HANDLE handle = 32u;
    D3DKMT_HANDLE resourceHandle = 32u;
    uint64_t ntHandle = 0u;
    Gmm gmm(executionEnvironment->rootDeviceEnvironments[0]->getGmmHelper(), nullptr, 20, 0, GMM_RESOURCE_USAGE_OCL_BUFFER, true, {}, true);

    EXPECT_NE(STATUS_SUCCESS, wddm->createAllocation(nullptr, &gmm, handle, resourceHandle, &ntHandle));
    EXPECT_EQ(wddm->destroyAllocationResult.called++, 1u);
    EXPECT_EQ(handle, 0u);
    EXPECT_EQ(resourceHandle, 0u);
}
