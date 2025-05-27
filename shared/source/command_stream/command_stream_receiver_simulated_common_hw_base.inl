/*
 * Copyright (C) 2019-2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/aub/aub_helper.h"
#include "shared/source/aub_mem_dump/page_table_entry_bits.h"
#include "shared/source/command_stream/command_stream_receiver_simulated_common_hw.h"
#include "shared/source/debug_settings/debug_settings_manager.h"
#include "shared/source/execution_environment/execution_environment.h"
#include "shared/source/execution_environment/root_device_environment.h"
#include "shared/source/gmm_helper/gmm.h"
#include "shared/source/gmm_helper/gmm_helper.h"
#include "shared/source/gmm_helper/resource_info.h"
#include "shared/source/helpers/aligned_memory.h"
#include "shared/source/helpers/basic_math.h"
#include "shared/source/helpers/hardware_context_controller.h"
#include "shared/source/memory_manager/memory_manager.h"
#include "shared/source/os_interface/os_context.h"

#include "aubstream/aub_manager.h"
#include "aubstream/aubstream.h"

namespace NEO {

template <typename GfxFamily>
void CommandStreamReceiverSimulatedCommonHw<GfxFamily>::setupContext(OsContext &osContext) {
    CommandStreamReceiverHw<GfxFamily>::setupContext(osContext);

    auto engineType = osContext.getEngineType();
    uint32_t flags = 0;
    getCsTraits(engineType).setContextSaveRestoreFlags(flags);

    if (osContext.isPartOfContextGroup()) {
        constexpr uint32_t contextGroupBit = aub_stream::hardwareContextFlags::contextGroup;
        flags |= contextGroupBit;
    }

    if (osContext.isHighPriority()) {
        flags |= aub_stream::hardwareContextFlags::highPriority;
    } else if (osContext.isLowPriority()) {
        flags |= aub_stream::hardwareContextFlags::lowPriority;
    }

    if (debugManager.flags.AppendAubStreamContextFlags.get() != -1) {
        flags |= static_cast<uint32_t>(debugManager.flags.AppendAubStreamContextFlags.get());
    }

    if (aubManager) {
        hardwareContextController = std::make_unique<HardwareContextController>(*aubManager, osContext, flags);
    }
}

template <typename GfxFamily>
bool CommandStreamReceiverSimulatedCommonHw<GfxFamily>::getParametersForMemory(GraphicsAllocation &graphicsAllocation, uint64_t &gpuAddress, void *&cpuAddress, size_t &size) const {
    cpuAddress = graphicsAllocation.getUnderlyingBuffer();
    gpuAddress = peekExecutionEnvironment().rootDeviceEnvironments[graphicsAllocation.getRootDeviceIndex()].get()->gmmHelper.get()->decanonize(graphicsAllocation.getGpuAddress());
    size = graphicsAllocation.getUnderlyingBufferSize();

    if (graphicsAllocation.isCompressionEnabled()) {
        size = graphicsAllocation.getDefaultGmm()->gmmResourceInfo->getSizeAllocation();
    }

    if (size == 0)
        return false;

    if (cpuAddress == nullptr && graphicsAllocation.isAllocationLockable()) {
        cpuAddress = this->getMemoryManager()->lockResource(&graphicsAllocation);
    }
    return true;
}

template <typename GfxFamily>
bool CommandStreamReceiverSimulatedCommonHw<GfxFamily>::expectMemoryEqual(void *gfxAddress, const void *srcAddress, size_t length) {
    return this->expectMemory(gfxAddress, srcAddress, length,
                              AubMemDump::CmdServicesMemTraceMemoryCompare::CompareOperationValues::CompareEqual);
}
template <typename GfxFamily>
bool CommandStreamReceiverSimulatedCommonHw<GfxFamily>::expectMemoryNotEqual(void *gfxAddress, const void *srcAddress, size_t length) {
    return this->expectMemory(gfxAddress, srcAddress, length,
                              AubMemDump::CmdServicesMemTraceMemoryCompare::CompareOperationValues::CompareNotEqual);
}
template <typename GfxFamily>
bool CommandStreamReceiverSimulatedCommonHw<GfxFamily>::expectMemoryCompressed(void *gfxAddress, const void *srcAddress, size_t length) {
    return this->expectMemory(gfxAddress, srcAddress, length,
                              AubMemDump::CmdServicesMemTraceMemoryCompare::CompareOperationValues::CompareNotEqual);
}

template <typename GfxFamily>
void CommandStreamReceiverSimulatedCommonHw<GfxFamily>::makeNonResident(GraphicsAllocation &gfxAllocation) {
    if (gfxAllocation.isResident(osContext->getContextId())) {
        dumpAllocation(gfxAllocation);
        this->getEvictionAllocations().push_back(&gfxAllocation);
        gfxAllocation.releaseResidencyInOsContext(this->osContext->getContextId());
    }
}

template <typename GfxFamily>
uint32_t CommandStreamReceiverSimulatedCommonHw<GfxFamily>::getDeviceIndex() const {
    return osContext->getDeviceBitfield().any() ? static_cast<uint32_t>(Math::log2(static_cast<uint32_t>(osContext->getDeviceBitfield().to_ulong()))) : 0u;
}
template <typename GfxFamily>
CommandStreamReceiverSimulatedCommonHw<GfxFamily>::CommandStreamReceiverSimulatedCommonHw(ExecutionEnvironment &executionEnvironment,
                                                                                          uint32_t rootDeviceIndex,
                                                                                          const DeviceBitfield deviceBitfield)
    : CommandStreamReceiverHw<GfxFamily>(executionEnvironment, rootDeviceIndex, deviceBitfield),
      releaseHelper(executionEnvironment.rootDeviceEnvironments[rootDeviceIndex]->getReleaseHelper()) {
    this->useNewResourceImplicitFlush = false;
    this->useGpuIdleImplicitFlush = false;
}
template <typename GfxFamily>
CommandStreamReceiverSimulatedCommonHw<GfxFamily>::~CommandStreamReceiverSimulatedCommonHw() {
    if (aubManager) {
        if (hardwareContextController) {
            for (auto &hardwareContext : hardwareContextController->hardwareContexts) {
                aubManager->releaseHardwareContext(hardwareContext.release());
            }
        }
    }
}
} // namespace NEO
