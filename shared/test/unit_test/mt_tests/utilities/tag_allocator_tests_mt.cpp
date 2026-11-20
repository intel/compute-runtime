/*
 * Copyright (C) 2026 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/helpers/constants.h"
#include "shared/source/helpers/timestamp_packet.h"
#include "shared/source/utilities/tag_allocator.h"
#include "shared/test/common/mocks/mock_execution_environment.h"
#include "shared/test/common/mocks/mock_graphics_allocation.h"
#include "shared/test/common/mocks/mock_memory_manager.h"

#include "gtest/gtest.h"

#include <array>
#include <atomic>
#include <set>
#include <thread>
#include <vector>

using namespace NEO;

namespace {
using MtTagType = TimestampPackets<uint32_t, TimestampPacketConstants::preferredPacketCount>;

class MtTagAllocator : public TagAllocator<MtTagType> {
  public:
    using BaseClass = TagAllocator<MtTagType>;
    using BaseClass::deferredTags;
    using BaseClass::freeTags;
    using BaseClass::releaseDeferredTags;
    using BaseClass::returnTagToDeferredPool;
    using BaseClass::tagPoolMemory;

    MtTagAllocator(MemoryManager *memoryManager, uint32_t tagCount)
        : BaseClass(RootDeviceIndicesContainer({0}), memoryManager, tagCount, MemoryConstants::cacheLineSize, sizeof(MtTagType),
                    TimestampPacketConstants::initValue, false, true, mockDeviceBitfield) {}
};
} // namespace

TEST(TagAllocatorMtTests, givenManyThreadsGettingAndReturningTagsWhenPoolsAreCreatedAndDeferredTagsReleasedConcurrentlyThenNoTagIsSharedAndAllTagsAreAccounted) {
    constexpr uint32_t tagCount = 4;
    constexpr uint32_t threadsCount = 8;
    constexpr uint32_t iterationsPerThread = 500;
    constexpr uint32_t tagsHeldPerThread = 3;
    constexpr uint32_t returnMethodsCount = 3;
    constexpr uint32_t doNotReleaseIterationStride = 100;
    constexpr uint32_t releaseDeferredIterationStride = 16;
    constexpr uint32_t markerThreadStride = iterationsPerThread * tagsHeldPerThread;

    MockExecutionEnvironment executionEnvironment;
    MockMemoryManager memoryManager(executionEnvironment);
    MtTagAllocator tagAllocator(&memoryManager, tagCount);

    std::atomic<uint32_t> missingTags{0};
    std::atomic<uint32_t> sharedTags{0};
    std::atomic<uint32_t> doNotReleaseTags{0};

    auto worker = [&](const uint32_t threadIndex) {
        std::array<TagNodeBase *, tagsHeldPerThread> heldTags{};
        for (uint32_t iteration = 0; iteration < iterationsPerThread; iteration++) {
            const uint32_t markerBase = threadIndex * markerThreadStride + iteration * tagsHeldPerThread;
            for (uint32_t tagIndex = 0; tagIndex < tagsHeldPerThread; tagIndex++) {
                heldTags[tagIndex] = tagAllocator.getTag();
                if (heldTags[tagIndex] == nullptr) {
                    missingTags++;
                    continue;
                }
                const uint32_t marker = markerBase + tagIndex;
                const std::array<uint32_t, 4> packet{marker, marker, marker, marker};
                heldTags[tagIndex]->assignDataToAllTimestamps(0, packet.data());
            }

            std::this_thread::yield();

            for (uint32_t tagIndex = 0; tagIndex < tagsHeldPerThread; tagIndex++) {
                auto tag = heldTags[tagIndex];
                if (tag == nullptr) {
                    continue;
                }
                if (tag->getContextStartValue(0) != markerBase + tagIndex || tag->getGlobalEndValue(0) != markerBase + tagIndex) {
                    sharedTags++;
                }
                const auto returnMethod = (iteration + tagIndex) % returnMethodsCount;
                if (returnMethod == 0) {
                    tag->returnTag();
                    continue;
                }
                if (returnMethod == 2 && iteration % doNotReleaseIterationStride == 0) {
                    tag->setDoNotReleaseNodes(true);
                    doNotReleaseTags++;
                }
                tag->refCountFetchSub(1);
                tagAllocator.returnTagToDeferredPool(tag);
            }

            if (iteration % releaseDeferredIterationStride == 0) {
                tagAllocator.releaseDeferredTags();
            }
        }
    };

    std::vector<std::thread> threads;
    for (uint32_t threadIndex = 0; threadIndex < threadsCount; threadIndex++) {
        threads.emplace_back(worker, threadIndex);
    }
    for (auto &thread : threads) {
        thread.join();
    }

    EXPECT_EQ(0u, missingTags.load());
    EXPECT_EQ(0u, sharedTags.load());

    tagAllocator.releaseDeferredTags();

    const size_t totalTagsCount = tagAllocator.tagPoolMemory.size() * tagCount;
    EXPECT_LT(1u, tagAllocator.tagPoolMemory.size());
    EXPECT_EQ(doNotReleaseTags.load(), tagAllocator.deferredTags.size());
    EXPECT_EQ(totalTagsCount, tagAllocator.freeTags.size() + tagAllocator.deferredTags.size());

    std::set<TagNodeBase *> uniqueTags(tagAllocator.freeTags.begin(), tagAllocator.freeTags.end());
    uniqueTags.insert(tagAllocator.deferredTags.begin(), tagAllocator.deferredTags.end());
    EXPECT_EQ(totalTagsCount, uniqueTags.size());
}
