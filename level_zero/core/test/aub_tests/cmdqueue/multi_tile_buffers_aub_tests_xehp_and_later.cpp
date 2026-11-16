/*
 * Copyright (C) 2026 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/helpers/constants.h"
#include "shared/test/common/test_macros/hw_test.h"
#include "shared/test/common/test_macros/test.h"

#include "level_zero/core/test/aub_tests/cmdqueue/multicontext_cmdqueue_l0_aub_fixture.h"

#include "multitile_matchers.h"

namespace L0 {
namespace ult {

struct MultiTileBuffersL0AubFixture : public MulticontextCmdQueueL0AubFixture {
    static constexpr uint32_t numTiles = 2;

    void setUp() {
        MulticontextCmdQueueL0AubFixture::setUp(numTiles, EnabledCommandStreamers::single, false);
    }
};

using MultiTileBuffersL0AubTests = Test<MultiTileBuffersL0AubFixture>;

HWTEST2_F(MultiTileBuffersL0AubTests, givenTwoBuffersAllocatedOnDifferentTilesWhenCopiedThenDataValidates, SupportsMultiTile) {
    constexpr size_t bufferSize = 64 * MemoryConstants::kiloByte;

    auto bufferTile0Memory = static_cast<uint8_t *>(allocHost(bufferSize));
    auto bufferTile1Memory = static_cast<uint8_t *>(allocHost(bufferSize));
    for (size_t index = 0; index < bufferSize; index++) {
        bufferTile0Memory[index] = static_cast<uint8_t>(index % 255);
        bufferTile1Memory[index] = static_cast<uint8_t>(255 - (index % 255));
    }

    auto srcBuffer = allocDevice(getTileDevice(0), bufferSize);
    auto dstBuffer = allocDevice(getTileDevice(1), bufferSize);

    auto &entry = queues[0][0];
    EXPECT_EQ(ZE_RESULT_SUCCESS, zeCommandListAppendMemoryCopy(entry.commandList, srcBuffer, bufferTile0Memory, bufferSize, nullptr, 0, nullptr));
    EXPECT_EQ(ZE_RESULT_SUCCESS, zeCommandListAppendMemoryCopy(entry.commandList, dstBuffer, bufferTile1Memory, bufferSize, nullptr, 0, nullptr));
    closeAndExecute(entry);
    synchronize(entry);

    expectMemory<FamilyType>(srcBuffer, bufferTile0Memory, bufferSize, 0, 0);
    expectMemory<FamilyType>(dstBuffer, bufferTile1Memory, bufferSize, 0, 0);

    EXPECT_EQ(ZE_RESULT_SUCCESS, zeCommandListReset(entry.commandList));
    EXPECT_EQ(ZE_RESULT_SUCCESS, zeCommandListAppendMemoryCopy(entry.commandList, dstBuffer, srcBuffer, bufferSize, nullptr, 0, nullptr));
    closeAndExecute(entry);
    synchronize(entry);

    expectMemory<FamilyType>(dstBuffer, bufferTile0Memory, bufferSize, 0, 0);
}

} // namespace ult
} // namespace L0
