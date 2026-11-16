/*
 * Copyright (C) 2026 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/command_stream/command_stream_receiver_simulated_common_hw.h"
#include "shared/source/helpers/constants.h"
#include "shared/source/helpers/hardware_context_controller.h"
#include "shared/source/helpers/ptr_math.h"
#include "shared/source/memory_manager/graphics_allocation.h"
#include "shared/source/memory_manager/unified_memory_manager.h"
#include "shared/test/common/test_macros/hw_test.h"
#include "shared/test/common/test_macros/test.h"

#include "level_zero/core/test/aub_tests/cmdqueue/multicontext_cmdqueue_l0_aub_fixture.h"

#include "aubstream/hardware_context.h"
#include "multitile_matchers.h"

#include <algorithm>

namespace L0 {
namespace ult {

struct OneVAFourPhysicalStoragesL0AubFixture : public MulticontextCmdQueueL0AubFixture {
    static constexpr uint32_t numTiles = 4;

    void setUp() {
        MulticontextCmdQueueL0AubFixture::setUp(numTiles, EnabledCommandStreamers::single, false);
    }

    NEO::GraphicsAllocation *getAllocation(void *ptr) {
        auto allocData = this->svmAllocsManager->getSVMAlloc(ptr);
        EXPECT_NE(nullptr, allocData);
        if (!allocData) {
            return nullptr;
        }
        return allocData->gpuAllocations.getGraphicsAllocation(this->rootDeviceIndex);
    }
};

using OneVAFourPhysicalStoragesL0AubTests = Test<OneVAFourPhysicalStoragesL0AubFixture>;

HWTEST2_F(OneVAFourPhysicalStoragesL0AubTests, givenBufferWithFourPhysicalStoragesWhenAppendingCopyToHostThenReadFromCorrectBank, SupportsMultiTile) {
    constexpr size_t bufferSize = MemoryConstants::pageSize64k;
    uint8_t *memoryToWrite[numTiles];
    uint8_t *memoryToRead[numTiles];

    auto buffer = allocDevice(rootDevice, bufferSize);
    auto allocation = getAllocation(buffer);
    ASSERT_NE(nullptr, allocation);
    EXPECT_EQ(NEO::MemoryPool::localMemory, allocation->getMemoryPool());
    auto gpuAddress = castToUint64(buffer);
    allocation->storageInfo.cloningOfPageTables = false;
    allocation->storageInfo.memoryBanks = 0;
    allocation->setAubWritable(false, static_cast<uint32_t>(maxNBitValue(numTiles)));

    for (uint32_t tile = 0; tile < numTiles; tile++) {
        memoryToWrite[tile] = static_cast<uint8_t *>(allocHost(bufferSize));
        std::fill(memoryToWrite[tile], memoryToWrite[tile] + bufferSize, static_cast<uint8_t>(tile + 1));

        auto hardwareContext = getSimulatedCsr<FamilyType>(tile, 0)->hardwareContextController->hardwareContexts[0].get();
        hardwareContext->writeMemory2({gpuAddress, memoryToWrite[tile], bufferSize, (1u << tile), aub_stream::DataTypeHintValues::TraceNotype, MemoryConstants::pageSize64k});
    }

    for (uint32_t tile = 0; tile < numTiles; tile++) {
        memoryToRead[tile] = static_cast<uint8_t *>(allocHost(bufferSize));

        auto &entry = queues[tile][0];
        EXPECT_EQ(ZE_RESULT_SUCCESS, zeCommandListAppendMemoryCopy(entry.commandList, memoryToRead[tile], buffer, bufferSize, nullptr, 0, nullptr));
        closeAndExecute(entry);
    }

    for (uint32_t tile = 0; tile < numTiles; tile++) {
        synchronize(queues[tile][0]);
        expectMemory<FamilyType>(memoryToRead[tile], memoryToWrite[tile], bufferSize, tile, 0);
    }
}

HWTEST2_F(OneVAFourPhysicalStoragesL0AubTests, whenAppendingCopyFromHostThenCorrectMemoryIsWrittenToSpecificBank, SupportsMultiTile) {
    constexpr size_t bufferSize = MemoryConstants::pageSize64k;
    uint8_t *memoryToWrite[numTiles];

    auto buffer = allocDevice(rootDevice, bufferSize);
    auto allocation = getAllocation(buffer);
    ASSERT_NE(nullptr, allocation);
    EXPECT_EQ(NEO::MemoryPool::localMemory, allocation->getMemoryPool());
    allocation->storageInfo.cloningOfPageTables = false;
    allocation->storageInfo.memoryBanks = 0;

    for (uint32_t tile = 0; tile < numTiles; tile++) {
        memoryToWrite[tile] = static_cast<uint8_t *>(allocHost(bufferSize));
        std::fill(memoryToWrite[tile], memoryToWrite[tile] + bufferSize, static_cast<uint8_t>(tile + 1));
        allocation->setAubWritable(true, 0xffffffff);

        auto &entry = queues[tile][0];
        EXPECT_EQ(ZE_RESULT_SUCCESS, zeCommandListAppendMemoryCopy(entry.commandList, buffer, memoryToWrite[tile], bufferSize, nullptr, 0, nullptr));
        closeAndExecute(entry);
        synchronize(entry);
    }

    for (uint32_t tile = 0; tile < numTiles; tile++) {
        expectMemory<FamilyType>(buffer, memoryToWrite[tile], bufferSize, tile, 0);
    }
}

HWTEST2_F(OneVAFourPhysicalStoragesL0AubTests, givenColouredBufferWhenAppendingCopyFromHostThenCorrectMemoryIsWrittenToSpecificBank, SupportsMultiTile) {
    constexpr size_t bufferSize = numTiles * MemoryConstants::pageSize64k;
    const auto allTilesValue = maxNBitValue(numTiles);
    auto memoryToWrite = static_cast<uint8_t *>(allocHost(bufferSize));

    auto buffer = allocDevice(rootDevice, bufferSize);
    auto allocation = getAllocation(buffer);
    ASSERT_NE(nullptr, allocation);
    EXPECT_EQ(NEO::MemoryPool::localMemory, allocation->getMemoryPool());
    EXPECT_EQ(allTilesValue, allocation->storageInfo.memoryBanks.to_ullong());
    EXPECT_EQ(allTilesValue, allocation->storageInfo.pageTablesVisibility.to_ullong());
    EXPECT_TRUE(allocation->storageInfo.cloningOfPageTables);

    for (uint32_t tile = 0; tile < numTiles; tile++) {
        std::fill(ptrOffset(memoryToWrite, tile * MemoryConstants::pageSize64k), ptrOffset(memoryToWrite, (tile + 1) * MemoryConstants::pageSize64k), static_cast<uint8_t>(tile + 1));
    }

    auto &entry = queues[0][0];
    EXPECT_EQ(ZE_RESULT_SUCCESS, zeCommandListAppendMemoryCopy(entry.commandList, buffer, memoryToWrite, bufferSize, nullptr, 0, nullptr));
    closeAndExecute(entry);
    synchronize(entry);

    for (uint32_t tile = 0; tile < numTiles; tile++) {
        for (size_t offset = 0; offset < bufferSize; offset += MemoryConstants::pageSize64k) {
            expectMemory<FamilyType>(ptrOffset(buffer, offset), ptrOffset(memoryToWrite, offset), MemoryConstants::pageSize64k, tile, 0);
        }
    }
}

} // namespace ult
} // namespace L0
