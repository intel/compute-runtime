/*
 * Copyright (C) 2026 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/indirect_heap/indirect_heap.h"
#include "shared/test/common/helpers/debug_manager_state_restore.h"
#include "shared/test/common/mocks/mock_bindless_heaps_helper.h"
#include "shared/test/common/mocks/mock_device.h"

#include "gtest/gtest.h"

#include <array>
#include <atomic>
#include <thread>

using namespace NEO;

TEST(BindlessHeapsHelperMtTest, givenConcurrentHeapRequestsWhenGettingHeapsThenEachHeapAndBorderColorStatesAreAllocatedOnce) {
    DebugManagerStateRestore dbgRestorer;
    debugManager.flags.UseExternalAllocatorForSshAndDsh.set(true);
    std::unique_ptr<MockDevice> rootDevice(MockDevice::createWithNewExecutionEnvironment<MockDevice>(defaultHwInfo.get(), 0u));
    auto bindlessHeapHelper = std::make_unique<MockBindlesHeapsHelper>(rootDevice.get(), false);

    constexpr uint32_t numThreads = 8;
    std::array<IndirectHeap *, numThreads> globalSshs = {};
    std::array<IndirectHeap *, numThreads> specialSshs = {};
    std::array<IndirectHeap *, numThreads> globalDshs = {};
    std::array<uint32_t, numThreads> borderColorOffsets = {};
    std::atomic_bool started = false;
    std::vector<std::thread> threads;
    for (uint32_t i = 0; i < numThreads; ++i) {
        threads.emplace_back([&, i]() {
            while (!started.load()) {
                std::this_thread::yield();
            }
            bindlessHeapHelper->allocateSSInHeap(bindlessHeapHelper->surfaceStateSize, nullptr, BindlessHeapsHelper::globalSsh);
            globalSshs[i] = bindlessHeapHelper->getHeap(BindlessHeapsHelper::globalSsh);
            specialSshs[i] = bindlessHeapHelper->getHeap(BindlessHeapsHelper::specialSsh);
            globalDshs[i] = bindlessHeapHelper->getHeap(BindlessHeapsHelper::globalDsh);
            borderColorOffsets[i] = bindlessHeapHelper->getDefaultBorderColorOffset();
        });
    }
    started = true;
    for (auto &thread : threads) {
        thread.join();
    }

    EXPECT_EQ(static_cast<size_t>(BindlessHeapsHelper::numHeapTypes), bindlessHeapHelper->ssHeapsAllocations.size());
    for (uint32_t i = 1; i < numThreads; ++i) {
        EXPECT_EQ(globalSshs[0], globalSshs[i]);
        EXPECT_EQ(specialSshs[0], specialSshs[i]);
        EXPECT_EQ(globalDshs[0], globalDshs[i]);
        EXPECT_EQ(borderColorOffsets[0], borderColorOffsets[i]);
    }
    EXPECT_EQ(numThreads * bindlessHeapHelper->surfaceStateSize, globalSshs[0]->getUsed());
}
