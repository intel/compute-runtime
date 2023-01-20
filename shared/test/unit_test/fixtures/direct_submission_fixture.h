/*
 * Copyright (C) 2020-2023 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once

#include "shared/source/command_stream/submissions_aggregator.h"
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
        DirectSubmissionFixture::setUp();
        MemoryManager *memoryManager = pDevice->getExecutionEnvironment()->memoryManager.get();
        const AllocationProperties commandBufferProperties{pDevice->getRootDeviceIndex(), 0x1000,
                                                           AllocationType::COMMAND_BUFFER, pDevice->getDeviceBitfield()};
        commandBuffer = memoryManager->allocateGraphicsMemoryWithProperties(commandBufferProperties);

        batchBuffer.endCmdPtr = &bbStart[0];
        batchBuffer.commandBufferAllocation = commandBuffer;
        batchBuffer.usedSize = 0x40;
        batchBuffer.taskStartAddress = 0x881112340000;
    }

    void tearDown() {
        MemoryManager *memoryManager = pDevice->getExecutionEnvironment()->memoryManager.get();
        memoryManager->freeGraphicsMemory(commandBuffer);

        DirectSubmissionFixture::tearDown();
    }

    BatchBuffer batchBuffer;
    uint8_t bbStart[64];
    GraphicsAllocation *commandBuffer;
};
