/*
 * Copyright (C) 2026 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/os_interface/linux/drm_allocation.h"
#include "shared/source/os_interface/linux/drm_buffer_object.h"
#include "shared/source/os_interface/linux/drm_memory_manager.h"
#include "shared/test/common/libult/linux/drm_mock.h"
#include "shared/test/common/mocks/linux/mock_drm_memory_manager.h"
#include "shared/test/common/mocks/mock_execution_environment.h"
#include "shared/test/common/os_interface/linux/device_command_stream_fixture.h"

#include "gtest/gtest.h"

using namespace NEO;

struct DrmAllocationViewTest : public ::testing::Test {
    void SetUp() override {
        executionEnvironment = std::make_unique<MockExecutionEnvironment>();
        executionEnvironment->rootDeviceEnvironments[0]->osInterface = std::make_unique<OSInterface>();

        drm = new DrmMock(*executionEnvironment->rootDeviceEnvironments[0]);
        executionEnvironment->rootDeviceEnvironments[0]->osInterface->setDriverModel(std::unique_ptr<DriverModel>(drm));

        memoryManager = std::make_unique<TestedDrmMemoryManager>(false, false, false, *executionEnvironment);

        parentAllocation = std::make_unique<DrmAllocation>(0u, 1u, AllocationType::buffer, nullptr, cpuPtr, gpuAddress, allocationSize, MemoryPool::localMemory);
    }

    const size_t allocationSize = 4096u;
    const size_t viewOffset = 1024u;
    const size_t viewSize = 512u;
    const uint64_t gpuAddress = 0x1000000u;
    void *cpuPtr = reinterpret_cast<void *>(0x2000000u);

    std::unique_ptr<MockExecutionEnvironment> executionEnvironment;
    std::unique_ptr<TestedDrmMemoryManager> memoryManager;
    DrmMock *drm = nullptr;
    std::unique_ptr<DrmAllocation> parentAllocation;
};

TEST_F(DrmAllocationViewTest, whenCreatingViewThenReturnsDrmAllocation) {
    std::unique_ptr<GraphicsAllocation> view(parentAllocation->createView(viewOffset, viewSize));

    ASSERT_NE(nullptr, view);
    EXPECT_TRUE(view->isView());
    EXPECT_EQ(parentAllocation.get(), view->getParentAllocation());
}

TEST_F(DrmAllocationViewTest, whenCreatingViewThenViewHasCorrectAddresses) {
    std::unique_ptr<GraphicsAllocation> view(parentAllocation->createView(viewOffset, viewSize));

    EXPECT_EQ(gpuAddress + viewOffset, view->getGpuAddress());
    EXPECT_EQ(ptrOffset(cpuPtr, viewOffset), view->getUnderlyingBuffer());
    EXPECT_EQ(viewSize, view->getUnderlyingBufferSize());
}

TEST_F(DrmAllocationViewTest, whenPeekInternalHandleOnViewThenDelegatesToParent) {
    auto bo = std::make_unique<BufferObject>(0u, drm, 3u, 0, 0, 1);
    parentAllocation->getBufferObjectToModify(0) = bo.get();

    auto view = std::unique_ptr<DrmAllocation>(static_cast<DrmAllocation *>(parentAllocation->createView(viewOffset, viewSize)));

    uint64_t handle = 0;
    int result = view->peekInternalHandle(memoryManager.get(), 0u, handle, nullptr);

    EXPECT_EQ(0, result);
    EXPECT_EQ(static_cast<uint64_t>(bo->peekHandle()), handle);

    parentAllocation->getBufferObjectToModify(0) = nullptr;
}

TEST_F(DrmAllocationViewTest, whenClearInternalHandleOnViewThenDelegatesToParent) {
    auto bo = std::make_unique<BufferObject>(0u, drm, 3u, 0, 0, 1);
    parentAllocation->getBufferObjectToModify(0) = bo.get();

    auto view = std::unique_ptr<DrmAllocation>(static_cast<DrmAllocation *>(parentAllocation->createView(viewOffset, viewSize)));

    view->clearInternalHandle(0u);

    parentAllocation->getBufferObjectToModify(0) = nullptr;
}

TEST_F(DrmAllocationViewTest, whenFreeingViewAllocationThenMemoryManagerReturnsEarly) {
    auto view = parentAllocation->createView(viewOffset, viewSize);

    memoryManager->freeGraphicsMemory(view);

    // View was not deleted by freeGraphicsMemory
    delete view;

    // Parent should still be valid
    EXPECT_FALSE(parentAllocation->isView());
    EXPECT_EQ(allocationSize, parentAllocation->getUnderlyingBufferSize());
}

TEST_F(DrmAllocationViewTest, whenCreatingViewThenViewInheritsBufferObjects) {
    auto bo = std::make_unique<BufferObject>(0u, drm, 3u, 0, 0, 1);
    parentAllocation->getBufferObjectToModify(0) = bo.get();

    auto view = std::unique_ptr<DrmAllocation>(static_cast<DrmAllocation *>(parentAllocation->createView(viewOffset, viewSize)));

    EXPECT_EQ(parentAllocation->getBO(), view->getBO());

    parentAllocation->getBufferObjectToModify(0) = nullptr;
}
