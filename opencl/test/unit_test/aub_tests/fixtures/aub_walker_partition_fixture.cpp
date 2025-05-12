/*
 * Copyright (C) 2022-2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "opencl/test/unit_test/aub_tests/fixtures/aub_walker_partition_fixture.h"

#include "shared/source/command_container/walker_partition_xehp_and_later.h"
#include "shared/source/helpers/array_count.h"
#include "shared/source/helpers/basic_math.h"
#include "shared/source/helpers/timestamp_packet.h"
#include "shared/source/utilities/io_functions.h"
#include "shared/source/utilities/tag_allocator.h"
#include "shared/test/common/cmd_parse/hw_parse.h"
#include "shared/test/common/helpers/debug_manager_state_restore.h"
#include "shared/test/common/helpers/dispatch_flags_helper.h"
#include "shared/test/common/mocks/mock_execution_environment.h"
#include "shared/test/common/test_macros/hw_test.h"

#include "opencl/source/command_queue/command_queue.h"
#include "opencl/source/event/event.h"

using namespace NEO;
using namespace WalkerPartition;

void AubWalkerPartitionFixture::setUp() {
    debugRestorer = std::make_unique<DebugManagerStateRestore>();
    debugManager.flags.EnableTimestampPacket.set(1);
    kernelIds |= (1 << 5);
    KernelAUBFixture<SimpleKernelFixture>::setUp();

    size_t userMemorySize = 16 * MemoryConstants::kiloByte;
    if (generateRandomInput) {
        userMemorySize = 16000 * MemoryConstants::kiloByte;
    }

    sizeUserMemory = userMemorySize;
    auto destMemory = alignedMalloc(sizeUserMemory, 4096);
    ASSERT_NE(nullptr, destMemory);
    memset(destMemory, 0x0, sizeUserMemory);

    dstBuffer.reset(Buffer::create(context, CL_MEM_COPY_HOST_PTR, sizeUserMemory, destMemory, retVal));
    ASSERT_NE(nullptr, dstBuffer);
    alignedFree(destMemory);

    kernels[5]->setArg(0, dstBuffer.get());
}

void AubWalkerPartitionFixture::tearDown() {
    pCmdQ->flush();

    KernelAUBFixture<SimpleKernelFixture>::tearDown();
}

void AubWalkerPartitionTest::SetUp() {
    AubWalkerPartitionFixture::setUp();
    std::tie(partitionCount, partitionType, dispatchParamters, workingDimensions) = GetParam();

    if (generateRandomInput) {
        workingDimensions = (rand() % 3 + 1);
        partitionType = (rand() % 3 + 1);
        partitionCount = rand() % 16 + 1;

        // now generate dimensions that makes sense
        auto goodWorkingSizeGenerated = false;
        while (!goodWorkingSizeGenerated) {
            dispatchParamters.localWorkSize[0] = rand() % 128 + 1;
            dispatchParamters.localWorkSize[1] = rand() % 128 + 1;
            dispatchParamters.localWorkSize[2] = rand() % 128 + 1;
            auto totalWorkItemsInWorkgroup = 1;
            for (auto dimension = 0u; dimension < workingDimensions; dimension++) {
                totalWorkItemsInWorkgroup *= static_cast<uint32_t>(dispatchParamters.localWorkSize[dimension]);
            }
            if (totalWorkItemsInWorkgroup <= 1024) {
                dispatchParamters.globalWorkSize[0] = dispatchParamters.localWorkSize[0] * (rand() % 32 + 1);
                dispatchParamters.globalWorkSize[1] = dispatchParamters.localWorkSize[1] * (rand() % 32 + 1);
                dispatchParamters.globalWorkSize[2] = dispatchParamters.localWorkSize[2] * (rand() % 32 + 1);

                printf("\n generated following dispatch paramters work dim %u gws %zu %zu %zu lws %zu %zu %zu, partition type %d partitionCount %d",
                       workingDimensions,
                       dispatchParamters.globalWorkSize[0],
                       dispatchParamters.globalWorkSize[1],
                       dispatchParamters.globalWorkSize[2],
                       dispatchParamters.localWorkSize[0],
                       dispatchParamters.localWorkSize[1],
                       dispatchParamters.localWorkSize[2],
                       partitionType,
                       partitionCount);
                IoFunctions::fflushPtr(stdout);
                goodWorkingSizeGenerated = true;
            }
        };
    }

    debugManager.flags.ExperimentalSetWalkerPartitionCount.set(partitionCount);
    debugManager.flags.ExperimentalSetWalkerPartitionType.set(partitionType);
    debugManager.flags.EnableWalkerPartition.set(1u);
}

void AubWalkerPartitionTest::TearDown() {
    AubWalkerPartitionFixture::tearDown();
}

void AubWalkerPartitionZeroFixture::setUp() {
    AubWalkerPartitionFixture::setUp();

    partitionCount = 0;
    partitionType = 0;

    workingDimensions = 1;

    debugManager.flags.ExperimentalSetWalkerPartitionCount.set(0);
    debugManager.flags.ExperimentalSetWalkerPartitionType.set(0);

    commandBufferProperties = std::make_unique<AllocationProperties>(device->getRootDeviceIndex(), true, MemoryConstants::pageSize, AllocationType::commandBuffer, false, device->getDeviceBitfield());
    auto memoryManager = this->device->getMemoryManager();
    streamAllocation = memoryManager->allocateGraphicsMemoryWithProperties(*commandBufferProperties);
    helperSurface = memoryManager->allocateGraphicsMemoryWithProperties(*commandBufferProperties);
    memset(helperSurface->getUnderlyingBuffer(), 0, MemoryConstants::pageSize);
    taskStream = std::make_unique<LinearStream>(streamAllocation);
}
void AubWalkerPartitionZeroFixture::tearDown() {
    auto memoryManager = this->device->getMemoryManager();
    memoryManager->freeGraphicsMemory(streamAllocation);
    memoryManager->freeGraphicsMemory(helperSurface);
    AubWalkerPartitionFixture::tearDown();
}

void AubWalkerPartitionZeroFixture::flushStream() {
    DispatchFlags dispatchFlags = DispatchFlagsHelper::createDefaultDispatchFlags();
    dispatchFlags.guardCommandBufferWithPipeControl = true;

    csr->makeResident(*helperSurface);
    csr->flushTask(*taskStream, 0,
                   &csr->getIndirectHeap(IndirectHeap::Type::dynamicState, 0u),
                   &csr->getIndirectHeap(IndirectHeap::Type::indirectObject, 0u),
                   &csr->getIndirectHeap(IndirectHeap::Type::surfaceState, 0u),
                   0u, dispatchFlags, device->getDevice());

    csr->flushBatchedSubmissions();
}
