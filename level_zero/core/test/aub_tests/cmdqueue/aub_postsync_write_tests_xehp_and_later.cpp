/*
 * Copyright (C) 2026 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/command_container/cmdcontainer.h"
#include "shared/test/common/cmd_parse/hw_parse.h"
#include "shared/test/common/test_macros/hw_test.h"
#include "shared/test/common/test_macros/test.h"

#include "level_zero/core/source/context/context.h"
#include "level_zero/core/source/device/device.h"
#include "level_zero/core/source/event/event.h"
#include "level_zero/core/test/aub_tests/fixtures/aub_fixture.h"
#include "level_zero/core/test/unit_tests/mocks/mock_cmdlist.h"

#include <algorithm>
#include <vector>

namespace L0 {
namespace ult {

struct PostSyncWriteL0AubFixture : public AUBFixtureL0 {
    void tearDown() {
        for (auto event : this->events) {
            zeEventDestroy(event);
        }
        if (this->eventPool) {
            zeEventPoolDestroy(this->eventPool);
        }
        AUBFixtureL0::tearDown();
    }

    void createEvents(ze_event_pool_flags_t poolFlags, uint32_t count) {
        ze_event_pool_desc_t eventPoolDesc = {ZE_STRUCTURE_TYPE_EVENT_POOL_DESC};
        eventPoolDesc.count = count;
        eventPoolDesc.flags = poolFlags;
        auto deviceHandle = this->device->toHandle();
        ASSERT_EQ(ZE_RESULT_SUCCESS, zeEventPoolCreate(this->context->toHandle(), &eventPoolDesc, 1, &deviceHandle, &this->eventPool));

        for (uint32_t i = 0; i < count; i++) {
            ze_event_desc_t eventDesc = {ZE_STRUCTURE_TYPE_EVENT_DESC};
            eventDesc.index = i;
            eventDesc.signal = ZE_EVENT_SCOPE_FLAG_HOST;
            eventDesc.wait = ZE_EVENT_SCOPE_FLAG_HOST;
            ze_event_handle_t event = nullptr;
            ASSERT_EQ(ZE_RESULT_SUCCESS, zeEventCreate(this->eventPool, &eventDesc, &event));
            this->events.push_back(event);
        }
    }

    template <typename FamilyType>
    void expectTimestampsWritten(ze_event_handle_t hEvent, bool checkStartTimestamps) {
        using TimestampPacketType = typename FamilyType::TimestampPacketType;
        const auto initialValue = static_cast<TimestampPacketType>(Event::STATE_INITIAL);

        auto event = Event::fromHandle(hEvent);
        const auto packetsInUse = event->getPacketsInUse();
        EXPECT_LE(1u, packetsInUse);

        for (uint32_t packet = 0; packet < packetsInUse; packet++) {
            const auto packetAddress = event->getGpuAddress(this->device) + packet * event->getSinglePacketSize();
            std::vector<size_t> offsets = {event->getContextEndOffset(), event->getGlobalEndOffset()};
            if (checkStartTimestamps) {
                offsets.push_back(event->getContextStartOffset());
                offsets.push_back(event->getGlobalStartOffset());
            }
            for (auto offset : offsets) {
                this->expectNotEqualMemory<FamilyType>(reinterpret_cast<void *>(packetAddress + offset), &initialValue, sizeof(TimestampPacketType));
            }
        }
    }

    ze_event_pool_handle_t eventPool = nullptr;
    std::vector<ze_event_handle_t> events;
};

using PostSyncWriteL0AubTests = Test<PostSyncWriteL0AubFixture>;

HWCMDTEST_F(IGFX_XE_HP_CORE, PostSyncWriteL0AubTests, givenTimestampEventWhenAppendingMemoryCopyThenWritePostsyncOperation) {
    constexpr size_t bufferSize = 4;
    createEvents(ZE_EVENT_POOL_FLAG_KERNEL_TIMESTAMP | ZE_EVENT_POOL_FLAG_HOST_VISIBLE, 1);

    auto dstBuffer = allocateDeviceMemory(bufferSize, 1);
    auto writeData = static_cast<uint8_t *>(allocateHostMemory(bufferSize, 1));
    const uint8_t pattern[bufferSize] = {1, 2, 3, 4};
    std::copy(pattern, pattern + bufferSize, writeData);

    EXPECT_EQ(ZE_RESULT_SUCCESS, zeCommandListAppendMemoryCopy(commandList->toHandle(), dstBuffer, writeData, bufferSize, events[0], 0, nullptr));
    executeAndSynchronize();

    expectMemory<FamilyType>(dstBuffer, pattern, bufferSize);
    expectTimestampsWritten<FamilyType>(events[0], true);
}

HWCMDTEST_F(IGFX_XE_HP_CORE, PostSyncWriteL0AubTests, givenEventWithoutTimestampWhenAppendingMemoryCopyThenWritePostsyncOperationInImmWriteMode) {
    constexpr size_t bufferSize = 4;
    createEvents(ZE_EVENT_POOL_FLAG_HOST_VISIBLE, 1);

    auto dstBuffer = allocateDeviceMemory(bufferSize, 1);
    auto writeData = static_cast<uint8_t *>(allocateHostMemory(bufferSize, 1));
    const uint8_t pattern[bufferSize] = {1, 2, 3, 4};
    std::copy(pattern, pattern + bufferSize, writeData);

    EXPECT_EQ(ZE_RESULT_SUCCESS, zeCommandListAppendMemoryCopy(commandList->toHandle(), dstBuffer, writeData, bufferSize, events[0], 0, nullptr));
    executeAndSynchronize();

    expectMemory<FamilyType>(dstBuffer, pattern, bufferSize);

    auto event = Event::fromHandle(events[0]);
    EXPECT_FALSE(event->isEventTimestampFlagSet());
    const auto packetsInUse = event->getPacketsInUse();
    EXPECT_LE(1u, packetsInUse);

    const uint32_t expectedSignaledValue = Event::STATE_SIGNALED;
    for (uint32_t packet = 0; packet < packetsInUse; packet++) {
        const auto completionAddress = event->getCompletionFieldGpuAddress(device) + packet * event->getSinglePacketSize();
        expectMemory<FamilyType>(reinterpret_cast<void *>(completionAddress), &expectedSignaledValue, sizeof(expectedSignaledValue));
    }
}

HWCMDTEST_F(IGFX_XE_HP_CORE, PostSyncWriteL0AubTests, givenTwoCopiesWithEventDependencyWhenExecutedThenEndTimestampsOfBothEventsAreWritten) {
    constexpr size_t bufferSize = 1024;
    createEvents(ZE_EVENT_POOL_FLAG_KERNEL_TIMESTAMP | ZE_EVENT_POOL_FLAG_HOST_VISIBLE, 2);

    auto dstBuffer = allocateDeviceMemory(bufferSize, 1);
    auto writePattern1 = static_cast<uint8_t *>(allocateHostMemory(bufferSize, 1));
    auto writePattern2 = static_cast<uint8_t *>(allocateHostMemory(bufferSize, 1));
    std::fill(writePattern1, writePattern1 + bufferSize, 1);
    std::fill(writePattern2, writePattern2 + bufferSize, 2);

    auto cmdListHandle = commandList->toHandle();
    EXPECT_EQ(ZE_RESULT_SUCCESS, zeCommandListAppendMemoryCopy(cmdListHandle, dstBuffer, writePattern1, bufferSize, events[0], 0, nullptr));
    EXPECT_EQ(ZE_RESULT_SUCCESS, zeCommandListAppendMemoryCopy(cmdListHandle, dstBuffer, writePattern2, bufferSize, events[1], 1, &events[0]));
    executeAndSynchronize();

    expectMemory<FamilyType>(dstBuffer, writePattern2, bufferSize);
    expectTimestampsWritten<FamilyType>(events[0], false);
    expectTimestampsWritten<FamilyType>(events[1], false);
}

HWCMDTEST_F(IGFX_XE_HP_CORE, PostSyncWriteL0AubTests, givenUnalignedCopySplitIntoMultipleWalkersWhenAppendingWithTimestampEventThenWriteAllTimestamps) {
    constexpr size_t bufferSize = 70;
    constexpr size_t writeOffset = 1;
    constexpr size_t writeSize = bufferSize - 2;
    createEvents(ZE_EVENT_POOL_FLAG_KERNEL_TIMESTAMP | ZE_EVENT_POOL_FLAG_HOST_VISIBLE, 1);

    auto dstBuffer = static_cast<uint8_t *>(allocateDeviceMemory(bufferSize, 1));
    auto writeData = static_cast<uint8_t *>(allocateHostMemory(writeSize, 1));
    std::fill(writeData, writeData + writeSize, 7);

    auto &cmdStream = *commandList->getCmdContainer().getCommandStream();
    auto streamOffset = cmdStream.getUsed();

    EXPECT_EQ(ZE_RESULT_SUCCESS, zeCommandListAppendMemoryCopy(commandList->toHandle(), dstBuffer + writeOffset, writeData, writeSize, events[0], 0, nullptr));

    NEO::HardwareParse hwParser;
    hwParser.parseCommands<FamilyType>(cmdStream, streamOffset);
    auto walkers = findAll<typename FamilyType::DefaultWalkerType *>(hwParser.cmdList.begin(), hwParser.cmdList.end());
    EXPECT_EQ(2u, walkers.size());

    executeAndSynchronize();

    expectMemory<FamilyType>(dstBuffer + writeOffset, writeData, writeSize);
    expectTimestampsWritten<FamilyType>(events[0], true);
}

} // namespace ult
} // namespace L0
