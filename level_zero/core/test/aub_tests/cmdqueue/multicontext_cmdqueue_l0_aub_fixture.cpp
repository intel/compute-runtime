/*
 * Copyright (C) 2026 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "level_zero/core/test/aub_tests/cmdqueue/multicontext_cmdqueue_l0_aub_fixture.h"

#include "shared/source/debug_settings/debug_settings_manager.h"
#include "shared/source/device/device.h"
#include "shared/source/helpers/constants.h"
#include "shared/source/helpers/definitions/engine_group_types.h"

#include "level_zero/core/source/cmdqueue/cmdqueue.h"
#include "level_zero/core/source/device/device.h"
#include "level_zero/core/source/driver/driver_handle.h"

#include "gtest/gtest.h"

#include <limits>

namespace L0 {
namespace ult {

void MulticontextCmdQueueL0AubFixture::setUp(uint32_t numberOfTiles, EnabledCommandStreamers enabledCommandStreamers, bool implicitScaling) {
    NEO::debugManager.flags.EnableDeviceUsmAllocationPool.set(0);
    NEO::debugManager.flags.EnableHostUsmAllocationPool.set(0);
    MulticontextL0AubFixture::setUp(numberOfTiles, enabledCommandStreamers, implicitScaling);
    if (::testing::Test::IsSkipped()) {
        return;
    }

    ze_context_desc_t contextDesc = {ZE_STRUCTURE_TYPE_CONTEXT_DESC};
    ASSERT_EQ(ZE_RESULT_SUCCESS, this->driverHandle->createContext(&contextDesc, 0u, nullptr, &this->contextHandle));

    this->queues.resize(numberOfTiles);
    for (uint32_t tile = 0; tile < numberOfTiles; tile++) {
        auto tileDevice = getTileDevice(tile);
        auto &tileQueues = this->queues[tile];

        if (EnabledCommandStreamers::single == enabledCommandStreamers) {
            auto engineGroupType = this->isRenderEngineSupported ? NEO::EngineGroupType::renderCompute : NEO::EngineGroupType::compute;
            tileQueues.push_back(createQueue(tileDevice, engineGroupType, 0));
        } else if (EnabledCommandStreamers::dual == enabledCommandStreamers) {
            if (this->isRenderEngineSupported) {
                tileQueues.push_back(createQueue(tileDevice, NEO::EngineGroupType::renderCompute, 0));
                tileQueues.push_back(createQueue(tileDevice, NEO::EngineGroupType::compute, 0));
            } else if (this->isCcs1Supported) {
                tileQueues.push_back(createQueue(tileDevice, NEO::EngineGroupType::compute, 0));
                tileQueues.push_back(createQueue(tileDevice, NEO::EngineGroupType::compute, 1));
            } else {
                tileQueues.push_back(createQueue(tileDevice, NEO::EngineGroupType::compute, 0));
                tileQueues.push_back(createQueue(tileDevice, NEO::EngineGroupType::compute, 0));
            }
        } else if (EnabledCommandStreamers::all == enabledCommandStreamers) {
            if (this->isRenderEngineSupported) {
                tileQueues.push_back(createQueue(tileDevice, NEO::EngineGroupType::renderCompute, 0));
            }
            for (uint32_t ccsIndex = 0; ccsIndex < 4; ccsIndex++) {
                tileQueues.push_back(createQueue(tileDevice, NEO::EngineGroupType::compute, ccsIndex));
            }
        }

        for (auto &entry : tileQueues) {
            ASSERT_NE(nullptr, entry.queue);
            ASSERT_NE(nullptr, entry.commandList);
            ASSERT_NE(nullptr, entry.csr);
        }
    }
}

void MulticontextCmdQueueL0AubFixture::tearDown() {
    auto destroyEntry = [](QueueEntry &entry) {
        if (entry.commandList) {
            zeCommandListDestroy(entry.commandList);
        }
        if (entry.queue) {
            zeCommandQueueDestroy(entry.queue);
        }
    };
    for (auto &tileQueues : this->queues) {
        for (auto &entry : tileQueues) {
            destroyEntry(entry);
        }
    }
    for (auto &entry : this->additionalQueues) {
        destroyEntry(entry);
    }
    for (auto ptr : this->allocations) {
        zeMemFree(this->contextHandle, ptr);
    }
    if (this->contextHandle) {
        zeContextDestroy(this->contextHandle);
    }
    MulticontextL0AubFixture::tearDown();
}

NEO::CommandStreamReceiver *MulticontextCmdQueueL0AubFixture::getGpgpuCsr(uint32_t tile, uint32_t engine) {
    return this->queues[tile][engine].csr;
}

L0::Device *MulticontextCmdQueueL0AubFixture::getTileDevice(uint32_t tile) const {
    return this->subDevices[tile] != nullptr ? this->subDevices[tile] : this->rootDevice;
}

MulticontextCmdQueueL0AubFixture::QueueEntry MulticontextCmdQueueL0AubFixture::createQueue(L0::Device *device, NEO::EngineGroupType engineGroupType, uint32_t engineIndex) {
    QueueEntry entry = {};
    const auto ordinal = static_cast<uint32_t>(device->getNEODevice()->getEngineGroupIndexFromEngineGroupType(engineGroupType));

    ze_command_queue_desc_t queueDesc = {ZE_STRUCTURE_TYPE_COMMAND_QUEUE_DESC};
    queueDesc.ordinal = ordinal;
    queueDesc.index = engineIndex;
    queueDesc.mode = ZE_COMMAND_QUEUE_MODE_ASYNCHRONOUS;
    EXPECT_EQ(ZE_RESULT_SUCCESS, zeCommandQueueCreate(this->contextHandle, device->toHandle(), &queueDesc, &entry.queue));

    ze_command_list_desc_t commandListDesc = {ZE_STRUCTURE_TYPE_COMMAND_LIST_DESC};
    commandListDesc.commandQueueGroupOrdinal = ordinal;
    EXPECT_EQ(ZE_RESULT_SUCCESS, zeCommandListCreate(this->contextHandle, device->toHandle(), &commandListDesc, &entry.commandList));

    if (entry.queue) {
        entry.csr = L0::CommandQueue::fromHandle(entry.queue)->getCsr();
    }
    return entry;
}

void *MulticontextCmdQueueL0AubFixture::allocDevice(L0::Device *device, size_t size) {
    ze_device_mem_alloc_desc_t deviceDesc = {ZE_STRUCTURE_TYPE_DEVICE_MEM_ALLOC_DESC};
    void *ptr = nullptr;
    EXPECT_EQ(ZE_RESULT_SUCCESS, zeMemAllocDevice(this->contextHandle, &deviceDesc, size, MemoryConstants::pageSize64k, device->toHandle(), &ptr));
    this->allocations.push_back(ptr);
    return ptr;
}

void *MulticontextCmdQueueL0AubFixture::allocHost(size_t size) {
    ze_host_mem_alloc_desc_t hostDesc = {ZE_STRUCTURE_TYPE_HOST_MEM_ALLOC_DESC};
    void *ptr = nullptr;
    EXPECT_EQ(ZE_RESULT_SUCCESS, zeMemAllocHost(this->contextHandle, &hostDesc, size, MemoryConstants::pageSize64k, &ptr));
    this->allocations.push_back(ptr);
    return ptr;
}

void MulticontextCmdQueueL0AubFixture::closeAndExecute(const QueueEntry &entry) {
    auto commandList = entry.commandList;
    EXPECT_EQ(ZE_RESULT_SUCCESS, zeCommandListClose(commandList));
    EXPECT_EQ(ZE_RESULT_SUCCESS, zeCommandQueueExecuteCommandLists(entry.queue, 1, &commandList, nullptr));
}

void MulticontextCmdQueueL0AubFixture::synchronize(const QueueEntry &entry) {
    EXPECT_EQ(ZE_RESULT_SUCCESS, zeCommandQueueSynchronize(entry.queue, std::numeric_limits<uint64_t>::max()));
}

} // namespace ult
} // namespace L0
