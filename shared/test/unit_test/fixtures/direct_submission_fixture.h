/*
 * Copyright (C) 2020-2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once

#include "shared/source/command_stream/submissions_aggregator.h"
#include "shared/source/helpers/compiler_product_helper.h"
#include "shared/source/memory_manager/allocation_properties.h"
#include "shared/source/memory_manager/memory_manager.h"
#include "shared/source/os_interface/device_factory.h"
#include "shared/source/os_interface/os_context.h"
#include "shared/test/common/fixtures/device_fixture.h"
#include "shared/test/common/helpers/engine_descriptor_helper.h"
#include "shared/test/common/mocks/mock_device.h"

#include <atomic>
#include <memory>

using namespace NEO;

namespace CpuIntrinsicsTests {
extern std::atomic<uintptr_t> lastClFlushedPtr;
}

struct DirectSubmissionFixture : public DeviceFixture {
    void setUp() {
        DeviceFixture::setUp();
        DeviceFactory::prepareDeviceEnvironments(*pDevice->getExecutionEnvironment());

        osContext = pDevice->getDefaultEngine().osContext;
    }

    OsContext *osContext = nullptr;
};

struct DirectSubmissionDispatchBufferFixture : public DirectSubmissionFixture {
    void setUp() {
        debugManager.flags.DirectSubmissionFlatRingBuffer.set(0);
        DirectSubmissionFixture::setUp();
        MemoryManager *memoryManager = pDevice->getExecutionEnvironment()->memoryManager.get();
        const AllocationProperties commandBufferProperties{pDevice->getRootDeviceIndex(), 0x1000,
                                                           AllocationType::commandBuffer, pDevice->getDeviceBitfield()};
        commandBuffer = memoryManager->allocateGraphicsMemoryWithProperties(commandBufferProperties);
        stream = std::make_unique<LinearStream>(commandBuffer);
        stream->getSpace(0x40);

        batchBuffer.endCmdPtr = &bbStart[0];
        batchBuffer.commandBufferAllocation = commandBuffer;
        batchBuffer.usedSize = 0x40;
        batchBuffer.taskStartAddress = 0x881112340000;
        batchBuffer.stream = stream.get();

        auto &compilerProductHelper = pDevice->getCompilerProductHelper();
        heaplessStateInit = compilerProductHelper.isHeaplessStateInitEnabled(compilerProductHelper.isHeaplessModeEnabled(*defaultHwInfo));
    }

    void tearDown() {
        MemoryManager *memoryManager = pDevice->getExecutionEnvironment()->memoryManager.get();
        memoryManager->freeGraphicsMemory(commandBuffer);

        DirectSubmissionFixture::tearDown();
    }

    BatchBuffer batchBuffer;
    uint8_t bbStart[64];
    GraphicsAllocation *commandBuffer;
    DebugManagerStateRestore restorer;
    std::unique_ptr<LinearStream> stream;

    bool heaplessStateInit = false;
};
