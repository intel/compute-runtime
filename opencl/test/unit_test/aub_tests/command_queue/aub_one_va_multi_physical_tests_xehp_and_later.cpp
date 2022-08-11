/*
 * Copyright (C) 2022 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/test/common/test_macros/hw_test.h"
#include "shared/test/common/utilities/base_object_utils.h"
#include "shared/test/unit_test/tests_configuration.h"

#include "opencl/source/mem_obj/buffer.h"
#include "opencl/test/unit_test/aub_tests/fixtures/multicontext_aub_fixture.h"

using namespace NEO;

struct OneVAFourPhysicalStoragesTest : public MulticontextAubFixture, public ::testing::Test {
    static const uint32_t numTiles = 4;
    void SetUp() override {
        MulticontextAubFixture::setUp(numTiles, MulticontextAubFixture::EnabledCommandStreamers::Single, false);
    }
    void TearDown() override {
        MulticontextAubFixture::tearDown();
    }
};

HWCMDTEST_F(IGFX_XE_HP_CORE, OneVAFourPhysicalStoragesTest, givenBufferWithFourPhysicalStoragesWhenEnqueueReadBufferThenReadFromCorrectBank) {
    if (is32bit) {
        return;
    }
    cl_int retVal = CL_OUT_OF_HOST_MEMORY;
    const uint32_t bufferSize = MemoryConstants::pageSize64k;
    uint8_t *memoryToWrite[numTiles];
    uint8_t *memoryToRead[numTiles];

    auto buffer = clUniquePtr<Buffer>(Buffer::create(context.get(), {}, bufferSize, nullptr, retVal));
    EXPECT_EQ(CL_SUCCESS, retVal);
    buffer->forceDisallowCPUCopy = true;
    auto allocation = buffer->getGraphicsAllocation(rootDeviceIndex);
    EXPECT_EQ(MemoryPool::LocalMemory, allocation->getMemoryPool());
    auto gpuAddress = allocation->getGpuAddress();
    allocation->storageInfo.cloningOfPageTables = false;
    allocation->storageInfo.memoryBanks = 0;
    allocation->setAubWritable(false, static_cast<uint32_t>(maxNBitValue(numTiles)));

    for (uint32_t tile = 0; tile < numTiles; tile++) {
        memoryToWrite[tile] = reinterpret_cast<uint8_t *>(alignedMalloc(bufferSize, MemoryConstants::pageSize64k));
        std::fill(memoryToWrite[tile], ptrOffset(memoryToWrite[tile], bufferSize), tile + 1);

        auto hardwareContext = getSimulatedCsr<FamilyType>(tile, 0)->hardwareContextController->hardwareContexts[0].get();
        hardwareContext->writeMemory2({gpuAddress, memoryToWrite[tile], bufferSize, (1u << tile), AubMemDump::DataTypeHintValues::TraceNotype, MemoryConstants::pageSize64k});
    }

    for (uint32_t tile = 0; tile < numTiles; tile++) {
        memoryToRead[tile] = reinterpret_cast<uint8_t *>(alignedMalloc(bufferSize, MemoryConstants::pageSize64k));

        commandQueues[tile][0]->enqueueReadBuffer(buffer.get(), CL_FALSE, 0, bufferSize, memoryToRead[tile], nullptr, 0, nullptr, nullptr);

        commandQueues[tile][0]->flush();
    }

    for (uint32_t tile = 0; tile < numTiles; tile++) {
        expectMemory<FamilyType>(memoryToRead[tile], memoryToWrite[tile], bufferSize, tile, 0);
        alignedFree(memoryToWrite[tile]);
        alignedFree(memoryToRead[tile]);
    }
}

HWCMDTEST_F(IGFX_XE_HP_CORE, OneVAFourPhysicalStoragesTest, givenBufferWithFourPhysicalStoragesWhenEnqueueWriteBufferThenCorrectMemoryIsWrittenToSpecificBank) {
    if (is32bit) {
        return;
    }
    cl_int retVal = CL_OUT_OF_HOST_MEMORY;
    const uint32_t bufferSize = MemoryConstants::pageSize64k;
    uint8_t *memoryToWrite[numTiles];

    auto buffer = clUniquePtr<Buffer>(Buffer::create(context.get(), {}, bufferSize, nullptr, retVal));
    EXPECT_EQ(CL_SUCCESS, retVal);
    buffer->forceDisallowCPUCopy = true;
    auto allocation = buffer->getGraphicsAllocation(rootDeviceIndex);
    EXPECT_EQ(MemoryPool::LocalMemory, allocation->getMemoryPool());
    auto gpuAddress = allocation->getGpuAddress();
    allocation->storageInfo.cloningOfPageTables = false;
    allocation->storageInfo.memoryBanks = 0;

    for (uint32_t tile = 0; tile < numTiles; tile++) {
        memoryToWrite[tile] = reinterpret_cast<uint8_t *>(alignedMalloc(bufferSize, MemoryConstants::pageSize64k));
        std::fill(memoryToWrite[tile], ptrOffset(memoryToWrite[tile], bufferSize), tile + 1);
        allocation->setAubWritable(true, 0xffffffff);

        commandQueues[tile][0]->enqueueWriteBuffer(buffer.get(), CL_TRUE, 0, bufferSize, memoryToWrite[tile], nullptr, 0, nullptr, nullptr);
    }

    for (uint32_t tile = 0; tile < numTiles; tile++) {
        expectMemory<FamilyType>(reinterpret_cast<void *>(gpuAddress), memoryToWrite[tile], bufferSize, tile, 0);
        alignedFree(memoryToWrite[tile]);
    }
}

HWCMDTEST_F(IGFX_XE_HP_CORE, OneVAFourPhysicalStoragesTest, givenColouredBufferWhenEnqueueWriteBufferThenCorrectMemoryIsWrittenToSpecificBank) {
    if (is32bit) {
        return;
    }

    cl_int retVal = CL_OUT_OF_HOST_MEMORY;
    const uint32_t bufferSize = numTiles * MemoryConstants::pageSize64k;
    const auto allTilesValue = maxNBitValue(numTiles);
    uint8_t *memoryToWrite = reinterpret_cast<uint8_t *>(alignedMalloc(bufferSize, MemoryConstants::pageSize64k));

    auto buffer = clUniquePtr<Buffer>(Buffer::create(context.get(), {}, bufferSize, nullptr, retVal));
    EXPECT_EQ(CL_SUCCESS, retVal);
    buffer->forceDisallowCPUCopy = true;
    auto allocation = buffer->getGraphicsAllocation(rootDeviceIndex);
    EXPECT_EQ(MemoryPool::LocalMemory, allocation->getMemoryPool());
    EXPECT_EQ(allTilesValue, allocation->storageInfo.memoryBanks.to_ullong());
    EXPECT_EQ(allTilesValue, allocation->storageInfo.pageTablesVisibility.to_ullong());
    EXPECT_TRUE(allocation->storageInfo.cloningOfPageTables);

    for (uint32_t tile = 0; tile < numTiles; tile++) {
        std::fill(ptrOffset(memoryToWrite, tile * MemoryConstants::pageSize64k), ptrOffset(memoryToWrite, (tile + 1) * MemoryConstants::pageSize64k), tile + 1);
    }

    commandQueues[0][0]->enqueueWriteBuffer(buffer.get(), CL_TRUE, 0, bufferSize, memoryToWrite, nullptr, 0, nullptr, nullptr);

    auto gpuAddress = allocation->getGpuAddress();
    for (uint32_t tile = 0; tile < numTiles; tile++) {
        for (uint32_t offset = 0; offset < bufferSize; offset += MemoryConstants::pageSize64k) {
            expectMemory<FamilyType>(reinterpret_cast<void *>(gpuAddress + offset), ptrOffset(memoryToWrite, offset), MemoryConstants::pageSize64k, tile, 0);
        }
    }

    alignedFree(memoryToWrite);
}
