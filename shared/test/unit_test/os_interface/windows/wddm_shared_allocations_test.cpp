/*
 * Copyright (C) 2021-2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/gmm_helper/gmm.h"
#include "shared/source/memory_manager/definitions/storage_info.h"
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

class WddmSharedTestsFixture : public MockExecutionEnvironmentGmmFixture {
  public:
    void setUp() {
        MockExecutionEnvironmentGmmFixture::setUp();
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

        auto &gfxCoreHelper = rootDeviceEnvironment->getHelper<GfxCoreHelper>();
        auto engine = gfxCoreHelper.getGpgpuEngineInstances(*rootDeviceEnvironment)[0];
        osContext = std::make_unique<OsContextWin>(*osInterface->getDriverModel()->as<Wddm>(), 0, 0u, EngineDescriptorHelper::getDefaultDescriptor(engine, preemptionMode));
        osContext->ensureContextInitialized(false);
    }

    void tearDown() {
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
    GmmRequirements gmmRequirements{};
    gmmRequirements.allowLargePages = true;
    gmmRequirements.preferCompressed = true;
    Gmm gmm(executionEnvironment->rootDeviceEnvironments[0]->getGmmHelper(), nullptr, 20, 0, GMM_RESOURCE_USAGE_OCL_BUFFER, {}, gmmRequirements);

    EXPECT_NE(STATUS_SUCCESS, wddm->createAllocation(nullptr, &gmm, handle, resourceHandle, &ntHandle));
    EXPECT_EQ(wddm->destroyAllocationResult.called++, 2u);
    EXPECT_EQ(handle, 0u);
    EXPECT_EQ(resourceHandle, 0u);
}

TEST_F(WdmmSharedTests, WhenCreatingSharedAllocationWithShareableWithoutNTHandleFlagThenNTHandleIsNotCreated) {
    init();

    D3DKMT_HANDLE handle = 32u;
    D3DKMT_HANDLE resourceHandle = 32u;
    uint64_t ntHandle = 0u;
    GmmRequirements gmmRequirements{};
    gmmRequirements.allowLargePages = true;
    gmmRequirements.preferCompressed = true;
    Gmm gmm(executionEnvironment->rootDeviceEnvironments[0]->getGmmHelper(), nullptr, 20, 0, GMM_RESOURCE_USAGE_OCL_BUFFER, {}, gmmRequirements);

    // Test with createNTHandle = false (shareableWithoutNTHandle = true)
    EXPECT_EQ(STATUS_SUCCESS, wddm->createAllocation(nullptr, &gmm, handle, resourceHandle, &ntHandle, false));
    EXPECT_NE(handle, 0u);
    EXPECT_NE(resourceHandle, 0u);
    EXPECT_EQ(ntHandle, 0u); // Should be set to 1 to indicate shared resource without NT handle
    EXPECT_EQ(wddm->destroyAllocationResult.called, 0u);
}

TEST_F(WdmmSharedTests, WhenCreatingSharedAllocationWithNormalShareableFlagThenNTHandleCreationIsAttempted) {
    init();

    D3DKMT_HANDLE handle = 32u;
    D3DKMT_HANDLE resourceHandle = 32u;
    uint64_t ntHandle = 0u;
    GmmRequirements gmmRequirements{};
    gmmRequirements.allowLargePages = true;
    gmmRequirements.preferCompressed = true;
    Gmm gmm(executionEnvironment->rootDeviceEnvironments[0]->getGmmHelper(), nullptr, 20, 0, GMM_RESOURCE_USAGE_OCL_BUFFER, {}, gmmRequirements);

    // Test with createNTHandle = true (shareableWithoutNTHandle = false)
    EXPECT_NE(STATUS_SUCCESS, wddm->createAllocation(nullptr, &gmm, handle, resourceHandle, &ntHandle, true));
    EXPECT_EQ(wddm->destroyAllocationResult.called, 2u); // Should be called because NT handle creation failed
    EXPECT_EQ(handle, 0u);
    EXPECT_EQ(resourceHandle, 0u);
}
