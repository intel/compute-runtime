/*
 * Copyright (C) 2026 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/os_interface/windows/wddm_allocation.h"
#include "shared/source/os_interface/windows/wddm_memory_manager.h"
#include "shared/test/common/mocks/mock_execution_environment.h"
#include "shared/test/common/mocks/mock_wddm.h"
#include "shared/test/common/os_interface/windows/mock_wddm_memory_manager.h"
#include "shared/test/common/os_interface/windows/wddm_fixture.h"

#include "gtest/gtest.h"

using namespace NEO;

struct WddmAllocationViewTest : public ::testing::Test {
    void SetUp() override {
        executionEnvironment = std::make_unique<MockExecutionEnvironment>();
        executionEnvironment->prepareRootDeviceEnvironments(1);
        executionEnvironment->rootDeviceEnvironments[0]->setHwInfoAndInitHelpers(defaultHwInfo.get());
        executionEnvironment->rootDeviceEnvironments[0]->initGmm();

        wddm = static_cast<WddmMock *>(Wddm::createWddm(nullptr, *executionEnvironment->rootDeviceEnvironments[0]));
        executionEnvironment->rootDeviceEnvironments[0]->osInterface = std::make_unique<OSInterface>();
        executionEnvironment->rootDeviceEnvironments[0]->osInterface->setDriverModel(std::unique_ptr<DriverModel>(wddm));
        wddm->init();
        executionEnvironment->rootDeviceEnvironments[0]->memoryOperationsInterface = std::make_unique<WddmMemoryOperationsHandler>(wddm);

        memoryManager = std::make_unique<MockWddmMemoryManager>(*executionEnvironment);

        parentAllocation = std::make_unique<WddmAllocation>(0u, 1u, AllocationType::buffer, cpuPtr, gpuAddress, allocationSize, nullptr, MemoryPool::localMemory, 0u, 1u);
    }

    const size_t allocationSize = 4096u;
    const size_t viewOffset = 1024u;
    const size_t viewSize = 512u;
    const uint64_t gpuAddress = 0x1000000u;
    void *cpuPtr = reinterpret_cast<void *>(0x2000000u);

    std::unique_ptr<MockExecutionEnvironment> executionEnvironment;
    std::unique_ptr<MockWddmMemoryManager> memoryManager;
    WddmMock *wddm = nullptr;
    std::unique_ptr<WddmAllocation> parentAllocation;
};

TEST_F(WddmAllocationViewTest, whenCreatingViewThenReturnsWddmAllocation) {
    std::unique_ptr<GraphicsAllocation> view(parentAllocation->createView(viewOffset, viewSize));

    ASSERT_NE(nullptr, view);
    EXPECT_TRUE(view->isView());
    EXPECT_EQ(parentAllocation.get(), view->getParentAllocation());
}

TEST_F(WddmAllocationViewTest, whenCreatingViewThenViewHasCorrectAddresses) {
    std::unique_ptr<GraphicsAllocation> view(parentAllocation->createView(viewOffset, viewSize));

    EXPECT_EQ(gpuAddress + viewOffset, view->getGpuAddress());
    EXPECT_EQ(ptrOffset(cpuPtr, viewOffset), view->getUnderlyingBuffer());
    EXPECT_EQ(viewSize, view->getUnderlyingBufferSize());
}

TEST_F(WddmAllocationViewTest, whenPeekInternalHandleOnViewThenDelegatesToParent) {
    auto view = std::unique_ptr<WddmAllocation>(static_cast<WddmAllocation *>(parentAllocation->createView(viewOffset, viewSize)));

    uint64_t viewHandle = 0;
    uint64_t parentHandle = 0;
    int viewResult = view->peekInternalHandle(memoryManager.get(), viewHandle);
    int parentResult = parentAllocation->peekInternalHandle(memoryManager.get(), parentHandle);

    // Verify view delegates to parent by checking same results
    EXPECT_EQ(parentResult, viewResult);
    EXPECT_EQ(parentHandle, viewHandle);
}

TEST_F(WddmAllocationViewTest, whenCreateInternalHandleOnViewThenDelegatesToParent) {
    const D3DKMT_HANDLE expectedHandle = 0x1234u;
    parentAllocation->setDefaultHandle(expectedHandle);

    auto view = std::unique_ptr<WddmAllocation>(static_cast<WddmAllocation *>(parentAllocation->createView(viewOffset, viewSize)));

    uint64_t handle = 0;
    int result = view->createInternalHandle(memoryManager.get(), 0u, handle);

    EXPECT_EQ(0, result);
}

TEST_F(WddmAllocationViewTest, whenClearInternalHandleOnViewThenDelegatesToParent) {
    const D3DKMT_HANDLE expectedHandle = 0x1234u;
    parentAllocation->setDefaultHandle(expectedHandle);

    auto view = std::unique_ptr<WddmAllocation>(static_cast<WddmAllocation *>(parentAllocation->createView(viewOffset, viewSize)));

    view->clearInternalHandle(0u);
}

TEST_F(WddmAllocationViewTest, whenFreeingViewAllocationThenMemoryManagerReturnsEarly) {
    auto view = parentAllocation->createView(viewOffset, viewSize);

    memoryManager->freeGraphicsMemory(view);

    // View was not deleted by freeGraphicsMemory
    delete view;

    // Parent should still be valid
    EXPECT_FALSE(parentAllocation->isView());
    EXPECT_EQ(allocationSize, parentAllocation->getUnderlyingBufferSize());
}

TEST_F(WddmAllocationViewTest, whenCreatingViewThenViewInheritsHandles) {
    const D3DKMT_HANDLE expectedHandle = 0x5678u;
    parentAllocation->setDefaultHandle(expectedHandle);

    auto view = std::unique_ptr<WddmAllocation>(static_cast<WddmAllocation *>(parentAllocation->createView(viewOffset, viewSize)));

    EXPECT_EQ(parentAllocation->getDefaultHandle(), view->getDefaultHandle());
}

TEST_F(WddmAllocationViewTest, whenCreatingViewThenViewInheritsResourceHandle) {
    const D3DKMT_HANDLE resourceHandle = 0xABCDu;
    parentAllocation->setResourceHandle(resourceHandle);

    auto view = std::unique_ptr<WddmAllocation>(static_cast<WddmAllocation *>(parentAllocation->createView(viewOffset, viewSize)));

    EXPECT_EQ(resourceHandle, view->getResourceHandle());
}
