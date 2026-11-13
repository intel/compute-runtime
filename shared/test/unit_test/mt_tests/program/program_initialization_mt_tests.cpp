/*
 * Copyright (C) 2026 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/memory_manager/unified_memory_pooling.h"
#include "shared/source/program/program_initialization.h"
#include "shared/source/utilities/shared_pool_allocation.h"
#include "shared/test/common/compiler_interface/linker_mock.h"
#include "shared/test/common/mocks/mock_device.h"
#include "shared/test/common/mocks/mock_memory_manager.h"
#include "shared/test/common/mocks/mock_product_helper.h"
#include "shared/test/common/mocks/mock_svm_manager.h"
#include "shared/test/common/mocks/mock_usm_memory_pool.h"

#include "gtest/gtest.h"

#include <atomic>
#include <functional>
#include <memory>
#include <thread>

using namespace NEO;

struct RacingInitializeUsmMemAllocPool : public MockUsmMemAllocPool {
    bool initialize(SVMAllocsManager *svmMemoryManager, const UnifiedMemoryProperties &memoryProperties, size_t poolSize, size_t minServicedSize, size_t maxServicedSize) override {
        if (this->initializeCalls.fetch_add(1) == 0) {
            this->onFirstInitialize();
        }
        return MockUsmMemAllocPool::initialize(svmMemoryManager, memoryProperties, poolSize, minServicedSize, maxServicedSize);
    }
    std::atomic<uint32_t> initializeCalls{0};
    std::function<void()> onFirstInitialize;
};

TEST(AllocateGlobalSurfaceMtTest, givenUsmGlobalSurfacePoolWhenAllocatingGlobalsSurfacesThenPoolIsInitializedOnceUnderPoolMutexAndBothSurfacesComeFromIt) {
    std::unique_ptr<MockSVMAllocsManager> svmAllocsManager;
    MockDevice device{};
    device.injectMemoryManager(new MockMemoryManager());
    auto mockProductHelper = new MockProductHelper;
    mockProductHelper->is2MBLocalMemAlignmentEnabledResult = true;
    device.getRootDeviceEnvironmentRef().productHelper.reset(mockProductHelper);
    svmAllocsManager = std::make_unique<MockSVMAllocsManager>(device.getMemoryManager());
    auto usmPool = new RacingInitializeUsmMemAllocPool;
    device.resetUsmGlobalSurfaceAllocPool(usmPool);

    WhiteBox<LinkerInput> linkerInput;
    linkerInput.traits.exportsGlobalVariables = true;
    std::vector<uint8_t> initData(64, 7u);

    bool poolMutexHeldDuringInitialize = false;
    usmPool->onFirstInitialize = [&]() {
        std::thread([&]() {
            auto acquired = usmPool->getMutex().try_lock();
            if (acquired) {
                usmPool->getMutex().unlock();
            }
            poolMutexHeldDuringInitialize = !acquired;
        }).join();
    };

    std::unique_ptr<SharedPoolAllocation> firstSurface(allocateGlobalsSurface(svmAllocsManager.get(), device, initData.size(), 0u, false, &linkerInput, initData.data()));
    std::unique_ptr<SharedPoolAllocation> secondSurface(allocateGlobalsSurface(svmAllocsManager.get(), device, initData.size(), 0u, false, &linkerInput, initData.data()));

    EXPECT_TRUE(poolMutexHeldDuringInitialize);
    EXPECT_EQ(1u, usmPool->initializeCalls.load());
    ASSERT_NE(nullptr, firstSurface);
    ASSERT_NE(nullptr, secondSurface);
    EXPECT_TRUE(firstSurface->isFromPool());
    EXPECT_TRUE(secondSurface->isFromPool());
    EXPECT_EQ(firstSurface->getGraphicsAllocation(), secondSurface->getGraphicsAllocation());
    EXPECT_TRUE(usmPool->isInPoolRange(reinterpret_cast<void *>(firstSurface->getGpuAddress())));
    EXPECT_TRUE(usmPool->isInPoolRange(reinterpret_cast<void *>(secondSurface->getGpuAddress())));
}
