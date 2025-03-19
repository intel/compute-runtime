/*
 * Copyright (C) 2023-2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once
#include "shared/source/aub/aub_center.h"
#include "shared/source/command_stream/command_stream_receiver.h"
#include "shared/source/execution_environment/execution_environment.h"
#include "shared/source/execution_environment/root_device_environment.h"
#include "shared/source/helpers/gfx_core_helper.h"
#include "shared/source/os_interface/aub_memory_operations_handler.h"

namespace NEO {
class Drm;

template <typename BaseOperationsHandler>
class DrmMemoryOperationsHandlerWithAubDump : public BaseOperationsHandler {
  public:
    DrmMemoryOperationsHandlerWithAubDump(RootDeviceEnvironment &rootDeviceEnvironment, uint32_t rootDeviceIndex)
        : BaseOperationsHandler(rootDeviceEnvironment, rootDeviceIndex) {

        if (!rootDeviceEnvironment.aubCenter) {
            auto &gfxCoreHelper = rootDeviceEnvironment.getHelper<GfxCoreHelper>();
            auto hardwareInfo = rootDeviceEnvironment.getMutableHardwareInfo();
            auto localMemoryEnabled = gfxCoreHelper.getEnableLocalMemory(*hardwareInfo);
            rootDeviceEnvironment.initGmm();
            rootDeviceEnvironment.initAubCenter(localMemoryEnabled, "", static_cast<CommandStreamReceiverType>(CommandStreamReceiverType::hardwareWithAub));
        }

        const auto aubCenter = rootDeviceEnvironment.aubCenter.get();
        aubMemoryOperationsHandler = std::make_unique<AubMemoryOperationsHandler>(aubCenter->getAubManager());
    };

    ~DrmMemoryOperationsHandlerWithAubDump() override = default;

    MemoryOperationsStatus makeResident(Device *device, ArrayRef<GraphicsAllocation *> gfxAllocations, bool isDummyExecNeeded, const bool forcePagingFence) override {
        aubMemoryOperationsHandler->makeResident(device, gfxAllocations, isDummyExecNeeded, forcePagingFence);
        return BaseOperationsHandler::makeResident(device, gfxAllocations, isDummyExecNeeded, forcePagingFence);
    }

    MemoryOperationsStatus lock(Device *device, ArrayRef<GraphicsAllocation *> gfxAllocations) override {
        aubMemoryOperationsHandler->makeResident(device, gfxAllocations, false, false);
        return BaseOperationsHandler::lock(device, gfxAllocations);
    }

    MemoryOperationsStatus evict(Device *device, GraphicsAllocation &gfxAllocation) override {
        aubMemoryOperationsHandler->evict(device, gfxAllocation);
        return BaseOperationsHandler::evict(device, gfxAllocation);
    }

    MemoryOperationsStatus free(Device *device, GraphicsAllocation &gfxAllocation) override {
        aubMemoryOperationsHandler->free(device, gfxAllocation);
        return BaseOperationsHandler::free(device, gfxAllocation);
    }

    MemoryOperationsStatus isResident(Device *device, GraphicsAllocation &gfxAllocation) override {
        aubMemoryOperationsHandler->isResident(device, gfxAllocation);
        return BaseOperationsHandler::isResident(device, gfxAllocation);
    }

    MemoryOperationsStatus makeResidentWithinOsContext(OsContext *osContext, ArrayRef<GraphicsAllocation *> gfxAllocations, bool evictable, const bool forcePagingFence, const bool acquireLock) override {
        aubMemoryOperationsHandler->makeResidentWithinOsContext(osContext, gfxAllocations, evictable, forcePagingFence, acquireLock);
        return BaseOperationsHandler::makeResidentWithinOsContext(osContext, gfxAllocations, evictable, forcePagingFence, acquireLock);
    }

    MemoryOperationsStatus evictWithinOsContext(OsContext *osContext, GraphicsAllocation &gfxAllocation) override {
        aubMemoryOperationsHandler->evictWithinOsContext(osContext, gfxAllocation);
        return BaseOperationsHandler::evictWithinOsContext(osContext, gfxAllocation);
    }

    void processFlushResidency(CommandStreamReceiver *csr) override {
        aubMemoryOperationsHandler->processFlushResidency(csr);
    }

  protected:
    std::unique_ptr<AubMemoryOperationsHandler> aubMemoryOperationsHandler;
};
} // namespace NEO
